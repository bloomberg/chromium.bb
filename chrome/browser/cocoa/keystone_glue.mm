// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/keystone_glue.h"

#include <sys/param.h>
#include <sys/mount.h>

#include <vector>

#import "app/l10n_util_mac.h"
#include "base/logging.h"
#include "base/mac_util.h"
#import "base/worker_pool_mac.h"
#include "chrome/browser/cocoa/authorization_util.h"
#include "chrome/common/chrome_constants.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"

namespace {

// Provide declarations of the Keystone registration bits needed here.  From
// KSRegistration.h.
typedef enum {
  kKSPathExistenceChecker,
} KSExistenceCheckerType;

typedef enum {
  kKSRegistrationUserTicket,
  kKSRegistrationSystemTicket,
  kKSRegistrationDontKnowWhatKindOfTicket,
} KSRegistrationTicketType;

NSString* KSRegistrationVersionKey = @"Version";
NSString* KSRegistrationExistenceCheckerTypeKey = @"ExistenceCheckerType";
NSString* KSRegistrationExistenceCheckerStringKey = @"ExistenceCheckerString";
NSString* KSRegistrationServerURLStringKey = @"URLString";
NSString* KSRegistrationPreserveTrustedTesterTokenKey = @"PreserveTTT";
NSString* KSRegistrationTagKey = @"Tag";
NSString* KSRegistrationTagPathKey = @"TagPath";
NSString* KSRegistrationTagKeyKey = @"TagKey";

NSString *KSRegistrationDidCompleteNotification =
    @"KSRegistrationDidCompleteNotification";
NSString *KSRegistrationPromotionDidCompleteNotification =
    @"KSRegistrationPromotionDidCompleteNotification";

NSString *KSRegistrationCheckForUpdateNotification =
    @"KSRegistrationCheckForUpdateNotification";
NSString *KSRegistrationStatusKey = @"Status";
NSString *KSRegistrationUpdateCheckErrorKey = @"Error";

NSString *KSRegistrationStartUpdateNotification =
    @"KSRegistrationStartUpdateNotification";
NSString *KSUpdateCheckSuccessfulKey = @"CheckSuccessful";
NSString *KSUpdateCheckSuccessfullyInstalledKey = @"SuccessfullyInstalled";

NSString *KSRegistrationRemoveExistingTag = @"";
#define KSRegistrationPreserveExistingTag nil

}  // namespace

@interface KSRegistration : NSObject

+ (id)registrationWithProductID:(NSString*)productID;

- (BOOL)registerWithParameters:(NSDictionary*)args;

- (BOOL)promoteWithParameters:(NSDictionary*)args
                authorization:(AuthorizationRef)authorization;

- (void)setActive;
- (void)checkForUpdate;
- (void)startUpdate;
- (KSRegistrationTicketType)ticketType;

@end  // @interface KSRegistration

@interface KeystoneGlue(Private)

// Returns the path to the application's Info.plist file.  This returns the
// outer application bundle's Info.plist, not the framework's Info.plist.
- (NSString*)appInfoPlistPath;

// Returns a dictionary containing parameters to be used for a KSRegistration
// -registerWithParameters: or -promoteWithParameters:authorization: call.
- (NSDictionary*)keystoneParameters;

// Called when Keystone registration completes.
- (void)registrationComplete:(NSNotification*)notification;

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
// thread and arranges for -determineUpdateStatus to be called on a work queue
// thread managed by NSOperationQueue.
// -determineUpdateStatus then reads the Info.plist, gets the version from the
// CFBundleShortVersionString key, and performs
// -determineUpdateStatusForVersion: on the main thread.
// -determineUpdateStatusForVersion: does the actual comparison of the version
// on disk with the running version and calls -updateStatus:version: with the
// results of its analysis.
- (void)determineUpdateStatusAsync;
- (void)determineUpdateStatus;
- (void)determineUpdateStatusForVersion:(NSString*)version;

// Returns YES if registration_ is definitely on a user ticket.  If definitely
// on a system ticket, or uncertain of ticket type (due to an older version
// of Keystone being used), returns NO.
- (BOOL)isUserTicket;

// Called when ticket promotion completes.
- (void)promotionComplete:(NSNotification*)notification;

// Changes the application's ownership and permissions so that all files are
// owned by root:wheel and all files and directories are writable only by
// root, but readable and executable as needed by everyone.
// -changePermissionsForPromotionAsync is called on the main thread by
// -promotionComplete.  That routine calls
// -changePermissionsForPromotionWithTool: on a work queue thread.  When done,
// -changePermissionsForPromotionComplete is called on the main thread.
- (void)changePermissionsForPromotionAsync;
- (void)changePermissionsForPromotionWithTool:(NSString*)toolPath;
- (void)changePermissionsForPromotionComplete;

@end  // @interface KeystoneGlue(Private)

const NSString* const kAutoupdateStatusNotification =
    @"AutoupdateStatusNotification";
const NSString* const kAutoupdateStatusStatus = @"status";
const NSString* const kAutoupdateStatusVersion = @"version";

namespace {

const NSString* const kChannelKey = @"KSChannelID";

}  // namespace

@implementation KeystoneGlue

+ (id)defaultKeystoneGlue {
  static bool sTriedCreatingDefaultKeystoneGlue = false;
  // TODO(jrg): use base::SingletonObjC<KeystoneGlue>
  static KeystoneGlue* sDefaultKeystoneGlue = nil;  // leaked

  if (!sTriedCreatingDefaultKeystoneGlue) {
    sTriedCreatingDefaultKeystoneGlue = true;

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
               selector:@selector(registrationComplete:)
                   name:KSRegistrationDidCompleteNotification
                 object:nil];

    [center addObserver:self
               selector:@selector(promotionComplete:)
                   name:KSRegistrationPromotionDidCompleteNotification
                 object:nil];

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
  [productID_ release];
  [appPath_ release];
  [url_ release];
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

  // Use [NSBundle mainBundle] to get the application's own bundle identifier
  // and path, not the framework's.  For auto-update, the application is
  // what's significant here: it's used to locate the outermost part of the
  // application for the existence checker and other operations that need to
  // see the entire application bundle.
  NSBundle* appBundle = [NSBundle mainBundle];

  NSString* productID = [infoDictionary objectForKey:@"KSProductID"];
  if (productID == nil) {
    productID = [appBundle bundleIdentifier];
  }

  NSString* appPath = [appBundle bundlePath];
  NSString* url = [infoDictionary objectForKey:@"KSUpdateURL"];
  NSString* version = [infoDictionary objectForKey:@"KSVersion"];

  if (!productID || !appPath || !url || !version) {
    // If parameters required for Keystone are missing, don't use it.
    return;
  }

  NSString* channel = [infoDictionary objectForKey:kChannelKey];
  // The stable channel has no tag.  If updating to stable, remove the
  // dev and beta tags since we've been "promoted".
  if (channel == nil)
    channel = KSRegistrationRemoveExistingTag;

  productID_ = [productID retain];
  appPath_ = [appPath retain];
  url_ = [url retain];
  version_ = [version retain];
  channel_ = [channel retain];
}

- (BOOL)loadKeystoneRegistration {
  if (!productID_ || !appPath_ || !url_ || !version_)
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

- (NSString*)appInfoPlistPath {
  // NSBundle ought to have a way to access this path directly, but it
  // doesn't.
  return [[appPath_ stringByAppendingPathComponent:@"Contents"]
             stringByAppendingPathComponent:@"Info.plist"];
}

- (NSDictionary*)keystoneParameters {
  NSNumber* xcType = [NSNumber numberWithInt:kKSPathExistenceChecker];
  NSNumber* preserveTTToken = [NSNumber numberWithBool:YES];
  NSString* tagPath = [self appInfoPlistPath];

  return [NSDictionary dictionaryWithObjectsAndKeys:
             version_, KSRegistrationVersionKey,
             xcType, KSRegistrationExistenceCheckerTypeKey,
             appPath_, KSRegistrationExistenceCheckerStringKey,
             url_, KSRegistrationServerURLStringKey,
             preserveTTToken, KSRegistrationPreserveTrustedTesterTokenKey,
             channel_, KSRegistrationTagKey,
             tagPath, KSRegistrationTagPathKey,
             kChannelKey, KSRegistrationTagKeyKey,
             nil];
}

- (void)registerWithKeystone {
  [self updateStatus:kAutoupdateRegistering version:nil];

  NSDictionary* parameters = [self keystoneParameters];
  if (![registration_ registerWithParameters:parameters]) {
    [self updateStatus:kAutoupdateRegisterFailed version:nil];
    return;
  }

  // Upon completion, KSRegistrationDidCompleteNotification will be posted,
  // and -registrationComplete: will be called.

  // Mark an active RIGHT NOW; don't wait an hour for the first one.
  [registration_ setActive];

  // Set up hourly activity pings.
  timer_ = [NSTimer scheduledTimerWithTimeInterval:60 * 60  // One hour
                                            target:self
                                          selector:@selector(markActive:)
                                          userInfo:registration_
                                           repeats:YES];
}

- (void)registrationComplete:(NSNotification*)notification {
  NSDictionary* userInfo = [notification userInfo];
  if ([[userInfo objectForKey:KSRegistrationStatusKey] boolValue]) {
    [self updateStatus:kAutoupdateRegistered version:nil];
  } else {
    // Dump registration_?
    [self updateStatus:kAutoupdateRegisterFailed version:nil];
  }
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

  if ([[userInfo objectForKey:KSRegistrationUpdateCheckErrorKey] boolValue]) {
    [self updateStatus:kAutoupdateCheckFailed version:nil];
  } else if ([[userInfo objectForKey:KSRegistrationStatusKey] boolValue]) {
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

  if (![[userInfo objectForKey:KSUpdateCheckSuccessfulKey] boolValue] ||
      ![[userInfo objectForKey:KSUpdateCheckSuccessfullyInstalledKey]
          intValue]) {
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
  DCHECK([NSThread isMainThread]);

  SEL selector = @selector(determineUpdateStatus);
  NSInvocationOperation* operation =
      [[[NSInvocationOperation alloc] initWithTarget:self
                                            selector:selector
                                              object:nil] autorelease];

  NSOperationQueue* operationQueue = [WorkerPoolObjC sharedOperationQueue];
  [operationQueue addOperation:operation];
}

// Runs on a thread managed by NSOperationQueue.
- (void)determineUpdateStatus {
  DCHECK(![NSThread isMainThread]);

  NSString* appInfoPlistPath = [self appInfoPlistPath];
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
  return status == kAutoupdateRegistering ||
         status == kAutoupdateChecking ||
         status == kAutoupdateInstalling ||
         status == kAutoupdatePromoting;
}

- (BOOL)isUserTicket {
  return [registration_ ticketType] == kKSRegistrationUserTicket;
}

- (BOOL)isOnReadOnlyFilesystem {
  const char* appPathC = [appPath_ fileSystemRepresentation];
  struct statfs statfsBuf;

  if (statfs(appPathC, &statfsBuf) != 0) {
    PLOG(ERROR) << "statfs";
    // Be optimistic about the filesystem's writability.
    return NO;
  }

  return (statfsBuf.f_flags & MNT_RDONLY) != 0;
}

- (BOOL)needsPromotion {
  if (![self isUserTicket] || [self isOnReadOnlyFilesystem]) {
    return NO;
  }

  // Check the outermost bundle directory, the main executable path, and the
  // framework directory.  It may be enough to just look at the outermost
  // bundle directory, but checking an interior file and directory can be
  // helpful in case permissions are set differently only on the outermost
  // directory.  An interior file and directory are both checked because some
  // file operations, such as Snow Leopard's Finder's copy operation when
  // authenticating, may actually result in different ownership being applied
  // to files and directories.
  NSFileManager* fileManager = [NSFileManager defaultManager];
  NSString* executablePath = [[NSBundle mainBundle] executablePath];
  NSString* frameworkPath = [mac_util::MainAppBundle() bundlePath];
  return ![fileManager isWritableFileAtPath:appPath_] ||
         ![fileManager isWritableFileAtPath:executablePath] ||
         ![fileManager isWritableFileAtPath:frameworkPath];
}

- (BOOL)wantsPromotion {
  // -needsPromotion checks these too, but this method doesn't necessarily
  // return NO just becuase -needsPromotion returns NO, so another check is
  // needed here.
  if (![self isUserTicket] || [self isOnReadOnlyFilesystem]) {
    return NO;
  }

  if ([self needsPromotion]) {
    return YES;
  }

  return [appPath_ hasPrefix:@"/Applications/"];
}

- (void)promoteTicket {
  if ([self asyncOperationPending] || ![self wantsPromotion]) {
    // Because there are multiple ways of reaching promoteTicket that might
    // not lock each other out, it may be possible to arrive here while an
    // asynchronous operation is pending, or even after promotion has already
    // occurred.  Just quietly return without doing anything.
    return;
  }

  // Create an empty AuthorizationRef.
  scoped_AuthorizationRef authorization;
  OSStatus status = AuthorizationCreate(NULL,
                                        kAuthorizationEmptyEnvironment,
                                        kAuthorizationFlagDefaults,
                                        &authorization);
  if (status != errAuthorizationSuccess) {
    LOG(ERROR) << "AuthorizationCreate: " << status;
    return;
  }

  // Specify the "system.privilege.admin" right, which allows
  // AuthorizationExecuteWithPrivileges to run commands as root.
  AuthorizationItem rightItems[] = {
    {kAuthorizationRightExecute, 0, NULL, 0}
  };
  AuthorizationRights rights = {arraysize(rightItems), rightItems};

  // product_logo_32.png is used instead of app.icns because Authorization
  // Services requires an image that NSImage can read.
  NSString* iconPath =
      [mac_util::MainAppBundle() pathForResource:@"product_logo_32"
                                          ofType:@"png"];
  const char* iconPathC = [iconPath fileSystemRepresentation];
  size_t iconPathLength = iconPathC ? strlen(iconPathC) : 0;

  // The OS will append " Type an administrator's name and password to allow
  // <CFBundleDisplayName> to make changes."
  NSString* prompt = l10n_util::GetNSStringFWithFixup(
      IDS_PROMOTE_AUTHENTICATION_PROMPT,
      l10n_util::GetStringUTF16(IDS_PRODUCT_NAME));
  const char* promptC = [prompt UTF8String];
  size_t promptLength = promptC ? strlen(promptC) : 0;

  AuthorizationItem environmentItems[] = {
    {kAuthorizationEnvironmentIcon, iconPathLength, (void*)iconPathC, 0},
    {kAuthorizationEnvironmentPrompt, promptLength, (void*)promptC, 0}
  };

  AuthorizationEnvironment environment = {arraysize(environmentItems),
                                          environmentItems};

  AuthorizationFlags flags = kAuthorizationFlagDefaults |
                             kAuthorizationFlagInteractionAllowed |
                             kAuthorizationFlagExtendRights |
                             kAuthorizationFlagPreAuthorize;

  status = AuthorizationCopyRights(authorization,
                                   &rights,
                                   &environment,
                                   flags,
                                   NULL);
  if (status != errAuthorizationSuccess) {
    if (status != errAuthorizationCanceled) {
      LOG(ERROR) << "AuthorizationCopyRights: " << status;
    }
    return;
  }

  [self updateStatus:kAutoupdatePromoting version:nil];

  // TODO(mark): Remove when able!
  //
  // keystone_promote_preflight is hopefully temporary.  It's here to ensure
  // that the Keystone system ticket store is in a usable state for all users
  // on the system.  Ideally, Keystone's installer or another part of Keystone
  // would handle this.  The underlying problem is http://b/2285921, and it
  // causes http://b/2289908, which this workaround addresses.
  //
  // This is run synchronously, which isn't optimal, but
  // -[KSRegistration promoteWithParameters:authorization:] is currently
  // synchronous too, and this operation needs to happen before that one.
  //
  // TODO(mark): Make asynchronous.  That only makes sense if the promotion
  // operation itself is asynchronous too.  http://b/2290009.  Hopefully,
  // the Keystone promotion code will just be changed to do what preflight
  // now does, and then the preflight script can be removed instead.
  NSString* preflightPath =
      [mac_util::MainAppBundle() pathForResource:@"keystone_promote_preflight"
                                          ofType:@"sh"];
  const char* preflightPathC = [preflightPath fileSystemRepresentation];
  const char* arguments[] = {NULL};

  int exit_status;
  status = authorization_util::ExecuteWithPrivilegesAndWait(
      authorization,
      preflightPathC,
      kAuthorizationFlagDefaults,
      arguments,
      NULL,  // pipe
      &exit_status);
  if (status != errAuthorizationSuccess) {
    LOG(ERROR) << "AuthorizationExecuteWithPrivileges preflight: " << status;
    [self updateStatus:kAutoupdatePromoteFailed version:nil];
    return;
  }
  if (exit_status != 0) {
    LOG(ERROR) << "keystone_promote_preflight status " << exit_status;
    [self updateStatus:kAutoupdatePromoteFailed version:nil];
    return;
  }

  // Hang on to the AuthorizationRef so that it can be used once promotion is
  // complete.  Do this before asking Keystone to promote the ticket, because
  // -promotionComplete: may be called from inside the Keystone promotion
  // call.
  authorization_.swap(authorization);

  NSDictionary* parameters = [self keystoneParameters];
  if (![registration_ promoteWithParameters:parameters
                              authorization:authorization_]) {
    [self updateStatus:kAutoupdatePromoteFailed version:nil];
    authorization_.reset();
    return;
  }

  // Upon completion, KSRegistrationPromotionDidCompleteNotification will be
  // posted, and -promotionComplete: will be called.
}

- (void)promotionComplete:(NSNotification*)notification {
  NSDictionary* userInfo = [notification userInfo];
  if ([[userInfo objectForKey:KSRegistrationStatusKey] boolValue]) {
    [self changePermissionsForPromotionAsync];
  } else {
    authorization_.reset();
    [self updateStatus:kAutoupdatePromoteFailed version:nil];
  }
}

- (void)changePermissionsForPromotionAsync {
  // NSBundle is not documented as being thread-safe.  Do NSBundle operations
  // on the main thread before jumping over to a NSOperationQueue-managed
  // thread to run the tool.
  DCHECK([NSThread isMainThread]);

  SEL selector = @selector(changePermissionsForPromotionWithTool:);
  NSString* toolPath =
      [mac_util::MainAppBundle() pathForResource:@"keystone_promote_postflight"
                                          ofType:@"sh"];

  NSInvocationOperation* operation =
      [[[NSInvocationOperation alloc] initWithTarget:self
                                            selector:selector
                                              object:toolPath] autorelease];

  NSOperationQueue* operationQueue = [WorkerPoolObjC sharedOperationQueue];
  [operationQueue addOperation:operation];
}

- (void)changePermissionsForPromotionWithTool:(NSString*)toolPath {
  const char* toolPathC = [toolPath fileSystemRepresentation];

  const char* appPathC = [appPath_ fileSystemRepresentation];
  const char* arguments[] = {appPathC, NULL};

  int exit_status;
  OSStatus status = authorization_util::ExecuteWithPrivilegesAndWait(
      authorization_,
      toolPathC,
      kAuthorizationFlagDefaults,
      arguments,
      NULL,  // pipe
      &exit_status);
  if (status != errAuthorizationSuccess) {
    LOG(ERROR) << "AuthorizationExecuteWithPrivileges postflight: " << status;
  } else if (exit_status != 0) {
    LOG(ERROR) << "keystone_promote_postflight status " << exit_status;
  }

  SEL selector = @selector(changePermissionsForPromotionComplete);
  [self performSelectorOnMainThread:selector
                         withObject:nil
                      waitUntilDone:NO];
}

- (void)changePermissionsForPromotionComplete {
  authorization_.reset();

  [self updateStatus:kAutoupdatePromoted version:nil];
}

@end  // @implementation KeystoneGlue
