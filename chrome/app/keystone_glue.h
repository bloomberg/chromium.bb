// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_APP_KEYSTONE_GLUE_H_
#define CHROME_APP_KEYSTONE_GLUE_H_

#import <Foundation/Foundation.h>
#import <base/scoped_nsobject.h>

// Possible outcomes of -checkForUpdate and -installUpdate.  A version may
// accompany some of these, but beware: a version is never required.  For
// statuses that can be accompanied by a version, the comment indicates what
// version is referenced.  A notification posted containing an asynchronous
// status will always be followed by a notification with a terminal status.
enum AutoupdateStatus {
  kAutoupdateNone = 0,      // no version (initial state only)
  kAutoupdateChecking,      // no version (asynchronous operation in progress)
  kAutoupdateCurrent,       // version of the running application
  kAutoupdateAvailable,     // version of the update that is available
  kAutoupdateInstalling,    // no version (asynchronous operation in progress)
  kAutoupdateInstalled,     // version of the update that was installed
  kAutoupdateCheckFailed,   // no version
  kAutoupdateInstallFailed  // no version
};

// kAutoupdateStatusNotification is the name of the notification posted when
// -checkForUpdate and -installUpdate complete.  This notification will be
// sent with with its sender object set to the KeystoneGlue instance sending
// the notification.  Its userInfo dictionary will contain an AutoupdateStatus
// value as an intValue at key kAutoupdateStatusStatus.  If a version is
// available (see AutoupdateStatus), it will be present at key
// kAutoupdateStatusVersion.
extern const NSString* const kAutoupdateStatusNotification;
extern const NSString* const kAutoupdateStatusStatus;
extern const NSString* const kAutoupdateStatusVersion;

// KeystoneGlue is an adapter around the KSRegistration class, allowing it to
// be used without linking directly against its containing KeystoneRegistration
// framework.  This is used in an environment where most builds (such as
// developer builds) don't want or need Keystone support and might not even
// have the framework available.  Enabling Keystone support in an application
// that uses KeystoneGlue is as simple as dropping
// KeystoneRegistration.framework in the application's Frameworks directory
// and providing the relevant information in its Info.plist.  KeystoneGlue
// requires that the KSUpdateURL key be set in the application's Info.plist,
// and that it contain a string identifying the update URL to be used by
// Keystone.

@class KSRegistration;

@interface KeystoneGlue : NSObject {
 @protected

  // Data for Keystone registration
  NSString* url_;
  NSString* productID_;
  NSString* version_;
  NSString* channel_;  // Logically: Dev, Beta, or Stable.

  // And the Keystone registration itself, with the active timer
  KSRegistration* registration_;  // strong
  NSTimer* timer_;  // strong

  // The most recent kAutoupdateStatusNotification notification posted.
  scoped_nsobject<NSNotification> recentNotification_;

  // YES if an update was ever successfully installed by -installUpdate.
  BOOL updateSuccessfullyInstalled_;
}

// Return the default Keystone Glue object.
+ (id)defaultKeystoneGlue;

// Load KeystoneRegistration.framework if present, call into it to register
// with Keystone, and set up periodic activity pings.
- (void)registerWithKeystone;

// -checkForUpdate launches a check for updates, and -installUpdate begins
// installing an available update.  For each, status will be communicated via
// a kAutoupdateStatusNotification notification, and will also be available
// through -recentNotification.
- (void)checkForUpdate;
- (void)installUpdate;

// Accessor for recentNotification_.  Returns an autoreleased NSNotification.
- (NSNotification*)recentNotification;

// Clears the saved recentNotification_.
- (void)clearRecentNotification;

// Accessor for the kAutoupdateStatusStatus field of recentNotification_'s
// userInfo dictionary.
- (AutoupdateStatus)recentStatus;

// Returns YES if an asynchronous operation is pending: if an update check or
// installation attempt is currently in progress.
- (BOOL)asyncOperationPending;

@end  // @interface KeystoneGlue

@interface KeystoneGlue(ExposedForTesting)

// Release the shared instance.  Use this in tests to reset the shared
// instance in case strange things are done to it for testing purposes.  Never
// call this from non-test code.
+ (void)releaseDefaultKeystoneGlue;

// Load any params we need for configuring Keystone.
- (void)loadParameters;

// Load the Keystone registration object.
// Return NO on failure.
- (BOOL)loadKeystoneRegistration;

- (void)stopTimer;

// Called when a checkForUpdate: notification completes.
- (void)checkForUpdateComplete:(NSNotification*)notification;

// Called when an installUpdate: notification completes.
- (void)installUpdateComplete:(NSNotification*)notification;

@end  // @interface KeystoneGlue(ExposedForTesting)

#endif  // CHROME_APP_KEYSTONE_GLUE_H_
