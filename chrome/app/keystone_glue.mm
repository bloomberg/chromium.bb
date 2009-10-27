// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/app/keystone_glue.h"

#include "base/logging.h"
#include "base/mac_util.h"
#import "base/worker_pool_mac.h"
#include "chrome/common/chrome_constants.h"

// Provide declarations of the Keystone registration bits needed here.  From
// KSRegistration.h.
typedef enum { kKSPathExistenceChecker } KSExistenceCheckerType;

NSString *KSRegistrationCheckForUpdateNotification =
    @"KSRegistrationCheckForUpdateNotification";
NSString *KSRegistrationStatusKey = @"Status";
NSString *KSRegistrationVersionKey = @"Version";

NSString *KSRegistrationStartUpdateNotification =
    @"KSRegistrationStartUpdateNotification";
NSString *KSUpdateCheckSuccessfulKey = @"CheckSuccessful";
NSString *KSUpdateCheckSuccessfullyInstalledKey = @"SuccessfullyInstalled";

NSString *KSRegistrationRemoveExistingTag = @"";
#define KSRegistrationPreserveExistingTag nil

@interface KSRegistration : NSObject

+ (id)registrationWithProductID:(NSString*)productID;

// Older API
- (BOOL)registerWithVersion:(NSString*)version
       existenceCheckerType:(KSExistenceCheckerType)xctype
     existenceCheckerString:(NSString*)xc
            serverURLString:(NSString*)serverURLString;
// Newer API
- (BOOL)registerWithVersion:(NSString*)version
       existenceCheckerType:(KSExistenceCheckerType)xctype
     existenceCheckerString:(NSString*)xc
            serverURLString:(NSString*)serverURLString
            preserveTTToken:(BOOL)preserveToken
                        tag:(NSString*)tag;

- (void)setActive;
- (void)checkForUpdate;
- (void)startUpdate;

@end  // @interface KSRegistration

@interface KeystoneGlue(Private)

// Called periodically to announce activity by pinging the Keystone server.
- (void)markActive:(NSTimer*)timer;

// Called when an update check or update installation is complete.  Posts the
// kAutoupdateStatusNotification notification to the default notification
// center.
- (void)updateStatus:(AutoupdateStatus)status version:(NSString*)version;

// These three methods are used to determine the version of the application
// currently installed on disk, compare that to the currently-running version,
// decide whether any updates have been installed, and call
// -updateStatus:version:.
//
// In order to check the version on disk, the installed application's
// Info.plist dictionary must be read; in order to see changes as updates are
// applied, the dictionary must be read each time, bypassing any caches such
// as the one that NSBundle might be maintaining.  Reading files can be a
// blocking operation, and blocking operations are to be avoided on the main
// thread.  I'm not quite sure what jank means, but I bet that a blocked main
// thread would cause some of it.
//
// -determineUpdateStatusAsync is called on the main thread to initiate the
// operation.  It performs initial set-up work that must be done on the main
// thread and arranges for -determineUpdateStatusAtPath: to be called on a
// work queue thread managed by NSOperationQueue.
// -determineUpdateStatusAtPath: then reads the Info.plist, gets the version
// from the CFBundleShortVersionString key, and performs
// -determineUpdateStatusForVersion: on the main thread.
// -determineUpdateStatusForVersion: does the actual comparison of the version
// on disk with the running version and calls -updateStatus:version: with the
// results of its analysis.
- (void)determineUpdateStatusAsync;
- (void)determineUpdateStatusAtPath:(NSString*)appPath;
- (void)determineUpdateStatusForVersion:(NSString*)version;

@end  // @interface KeystoneGlue(Private)

const NSString* const kAutoupdateStatusNotification =
    @"AutoupdateStatusNotification";
const NSString* const kAutoupdateStatusStatus = @"status";
const NSString* const kAutoupdateStatusVersion = @"version";

@implementation KeystoneGlue

// TODO(jrg): use base::SingletonObjC<KeystoneGlue>
static KeystoneGlue* sDefaultKeystoneGlue = nil;  // leaked

+ (id)defaultKeystoneGlue {
  if (!sDefaultKeystoneGlue) {
    sDefaultKeystoneGlue = [[KeystoneGlue alloc] init];
    [sDefaultKeystoneGlue loadParameters];
    if (![sDefaultKeystoneGlue loadKeystoneRegistration]) {
      [sDefaultKeystoneGlue release];
      sDefaultKeystoneGlue = nil;
    }
  }
  return sDefaultKeystoneGlue;
}

- (id)init {
  if ((self = [super init])) {
    NSNotificationCenter* center = [NSNotificationCenter defaultCenter];

    [center addObserver:self
               selector:@selector(checkForUpdateComplete:)
                   name:KSRegistrationCheckForUpdateNotification
                 object:nil];

    [center addObserver:self
               selector:@selector(installUpdateComplete:)
                   name:KSRegistrationStartUpdateNotification
                 object:nil];
  }

  return self;
}

- (void)dealloc {
  [url_ release];
  [productID_ release];
  [version_ release];
  [channel_ release];
  [registration_ release];
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  [super dealloc];
}

- (NSDictionary*)infoDictionary {
  // Use mac_util::MainAppBundle() to get the app framework's dictionary.
  return [mac_util::MainAppBundle() infoDictionary];
}

- (void)loadParameters {
  NSDictionary* infoDictionary = [self infoDictionary];
  NSString* url = [infoDictionary objectForKey:@"KSUpdateURL"];
  NSString* product = [infoDictionary objectForKey:@"KSProductID"];
  if (product == nil) {
    // Use [NSBundle mainBundle] to fall back to the app's own bundle
    // identifier, not the app framework's.
    product = [[NSBundle mainBundle] bundleIdentifier];
  }
  NSString* version = [infoDictionary objectForKey:@"KSVersion"];
  if (!product || !url || !version) {
    // If parameters required for Keystone are missing, don't use it.
    return;
  }

  NSString* channel = [infoDictionary objectForKey:@"KSChannelID"];
  // The stable channel has no tag.  If updating to stable, remove the
  // dev and beta tags since we've been "promoted".
  if (channel == nil)
    channel = KSRegistrationRemoveExistingTag;

  url_ = [url retain];
  productID_ = [product retain];
  version_ = [version retain];
  channel_ = [channel retain];
}

- (BOOL)loadKeystoneRegistration {
  if (!productID_ || !url_ || !version_)
    return NO;

  // Load the KeystoneRegistration framework bundle if present.  It lives
  // inside the framework, so use mac_util::MainAppBundle();
  NSString* ksrPath =
      [[mac_util::MainAppBundle() privateFrameworksPath]
          stringByAppendingPathComponent:@"KeystoneRegistration.framework"];
  NSBundle* ksrBundle = [NSBundle bundleWithPath:ksrPath];
  [ksrBundle load];

  // Harness the KSRegistration class.
  Class ksrClass = [ksrBundle classNamed:@"KSRegistration"];
  KSRegistration* ksr = [ksrClass registrationWithProductID:productID_];
  if (!ksr)
    return NO;

  registration_ = [ksr retain];
  return YES;
}

- (void)registerWithKeystone {
  // The existence checks should use the path to the app bundle, not the
  // app framework bundle, so use [NSBundle mainBundle] instead of
  // mac_util::MainBundle().
  //
  // Only use new API if we can.  This lets us land the new call
  // before the new Keystone has been released.
  //
  // TODO(jrg): once we hit Beta and the new Keystone is released,
  // make this call unconditional.
  if ([registration_ respondsToSelector:@selector(registerWithVersion:existenceCheckerType:existenceCheckerString:serverURLString:preserveTTToken:tag:)])
    [registration_ registerWithVersion:version_
                  existenceCheckerType:kKSPathExistenceChecker
                existenceCheckerString:[[NSBundle mainBundle] bundlePath]
                       serverURLString:url_
                       preserveTTToken:YES
                                   tag:channel_];
  else
    [registration_ registerWithVersion:version_
                  existenceCheckerType:kKSPathExistenceChecker
                existenceCheckerString:[[NSBundle mainBundle] bundlePath]
                       serverURLString:url_];

  // Mark an active RIGHT NOW; don't wait an hour for the first one.
  [registration_ setActive];

  // Set up hourly activity pings.
  timer_ = [NSTimer scheduledTimerWithTimeInterval:60 * 60  // One hour
                                            target:self
                                          selector:@selector(markActive:)
                                          userInfo:registration_
                                           repeats:YES];
}

- (void)stopTimer {
  [timer_ invalidate];
}

- (void)markActive:(NSTimer*)timer {
  KSRegistration* ksr = [timer userInfo];
  [ksr setActive];
}

- (void)checkForUpdate {
  DCHECK(![self asyncOperationPending]);

  if (!registration_) {
    [self updateStatus:kAutoupdateCheckFailed version:nil];
    return;
  }

  [self updateStatus:kAutoupdateChecking version:nil];

  [registration_ checkForUpdate];

  // Upon completion, KSRegistrationCheckForUpdateNotification will be posted,
  // and -checkForUpdateComplete: will be called.
}

- (void)checkForUpdateComplete:(NSNotification*)notification {
  NSDictionary* userInfo = [notification userInfo];
  BOOL updatesAvailable =
      [[userInfo objectForKey:KSRegistrationStatusKey] boolValue];

  if (updatesAvailable) {
    // If an update is known to be available, go straight to
    // -updateStatus:version:.  It doesn't matter what's currently on disk.
    NSString* version = [userInfo objectForKey:KSRegistrationVersionKey];
    [self updateStatus:kAutoupdateAvailable version:version];
  } else {
    // If no updates are available, check what's on disk, because an update
    // may have already been installed.  This check happens on another thread,
    // and -updateStatus:version: will be called on the main thread when done.
    [self determineUpdateStatusAsync];
  }
}

- (void)installUpdate {
  DCHECK(![self asyncOperationPending]);

  if (!registration_) {
    [self updateStatus:kAutoupdateInstallFailed version:nil];
    return;
  }

  [self updateStatus:kAutoupdateInstalling version:nil];

  [registration_ startUpdate];

  // Upon completion, KSRegistrationStartUpdateNotification will be posted,
  // and -installUpdateComplete: will be called.
}

- (void)installUpdateComplete:(NSNotification*)notification {
  NSDictionary* userInfo = [notification userInfo];
  BOOL checkSuccessful =
      [[userInfo objectForKey:KSUpdateCheckSuccessfulKey] boolValue];
  int installs =
      [[userInfo objectForKey:KSUpdateCheckSuccessfullyInstalledKey] intValue];

  if (!checkSuccessful || !installs) {
    [self updateStatus:kAutoupdateInstallFailed version:nil];
  } else {
    updateSuccessfullyInstalled_ = YES;

    // Nothing in the notification dictionary reports the version that was
    // installed.  Figure it out based on what's on disk.
    [self determineUpdateStatusAsync];
  }
}

// Runs on the main thread.
- (void)determineUpdateStatusAsync {
  // NSBundle is not documented as being thread-safe.  Do NSBundle operations
  // on the main thread before jumping over to a NSOperationQueue-managed
  // thread to do blocking file input.
  DCHECK([NSThread isMainThread]);

  SEL selector = @selector(determineUpdateStatusAtPath:);
  NSString* appPath = [[NSBundle mainBundle] bundlePath];
  NSInvocationOperation* operation =
      [[[NSInvocationOperation alloc] initWithTarget:self
                                            selector:selector
                                              object:appPath] autorelease];

  NSOperationQueue* operationQueue = [WorkerPoolObjC sharedOperationQueue];
  [operationQueue addOperation:operation];
}

// Runs on a thread managed by NSOperationQueue.
- (void)determineUpdateStatusAtPath:(NSString*)appPath {
  DCHECK(![NSThread isMainThread]);

  NSString* appInfoPlistPath =
      [[appPath stringByAppendingPathComponent:@"Contents"]
          stringByAppendingPathComponent:@"Info.plist"];
  NSDictionary* infoPlist =
      [NSDictionary dictionaryWithContentsOfFile:appInfoPlistPath];
  NSString* version = [infoPlist objectForKey:@"CFBundleShortVersionString"];

  [self performSelectorOnMainThread:@selector(determineUpdateStatusForVersion:)
                         withObject:version
                      waitUntilDone:NO];
}

// Runs on the main thread.
- (void)determineUpdateStatusForVersion:(NSString*)version {
  DCHECK([NSThread isMainThread]);

  AutoupdateStatus status;
  if (updateSuccessfullyInstalled_) {
    // If an update was successfully installed and this object saw it happen,
    // then don't even bother comparing versions.
    status = kAutoupdateInstalled;
  } else {
    NSString* currentVersion =
        [NSString stringWithUTF8String:chrome::kChromeVersion];
    if (!version) {
      // If the version on disk could not be determined, assume that
      // whatever's running is current.
      version = currentVersion;
      status = kAutoupdateCurrent;
    } else if ([version isEqualToString:currentVersion]) {
      status = kAutoupdateCurrent;
    } else {
      // If the version on disk doesn't match what's currently running, an
      // update must have been applied in the background, without this app's
      // direct participation.  Leave updateSuccessfullyInstalled_ alone
      // because there's no direct knowledge of what actually happened.
      status = kAutoupdateInstalled;
    }
  }

  [self updateStatus:status version:version];
}

- (void)updateStatus:(AutoupdateStatus)status version:(NSString*)version {
  NSNumber* statusNumber = [NSNumber numberWithInt:status];
  NSMutableDictionary* dictionary =
      [NSMutableDictionary dictionaryWithObject:statusNumber
                                         forKey:kAutoupdateStatusStatus];
  if (version) {
    [dictionary setObject:version forKey:kAutoupdateStatusVersion];
  }

  NSNotification* notification =
      [NSNotification notificationWithName:kAutoupdateStatusNotification
                                    object:self
                                  userInfo:dictionary];
  recentNotification_.reset([notification retain]);

  [[NSNotificationCenter defaultCenter] postNotification:notification];
}

- (NSNotification*)recentNotification {
  return [[recentNotification_ retain] autorelease];
}

- (AutoupdateStatus)recentStatus {
  NSDictionary* dictionary = [recentNotification_ userInfo];
  return static_cast<AutoupdateStatus>(
      [[dictionary objectForKey:kAutoupdateStatusStatus] intValue]);
}

- (BOOL)asyncOperationPending {
  AutoupdateStatus status = [self recentStatus];
  return status == kAutoupdateChecking || status == kAutoupdateInstalling;
}

@end  // @implementation KeystoneGlue
