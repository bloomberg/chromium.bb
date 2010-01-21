// Copyright (c) 2009-2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COCOA_CLEAR_BROWSING_DATA_CONTROLLER_
#define CHROME_BROWSER_COCOA_CLEAR_BROWSING_DATA_CONTROLLER_

#import <Cocoa/Cocoa.h>

#include "base/scoped_ptr.h"

class BrowsingDataRemover;
class ClearBrowsingObserver;
class Profile;
@class ThrobberView;

// Name of notification that is called when data is cleared.
extern NSString* const kClearBrowsingDataControllerDidDelete;
// A key in the above notification's userInfo. Contains a NSNumber with the
// logically-ored constants defined in BrowsingDataRemover for the removal.
extern NSString* const kClearBrowsingDataControllerRemoveMask;

// A window controller for managing the "Clear Browsing Data" feature. Modally
// presents a dialog offering the user a set of choices of what browsing data
// to delete and does so if the user chooses.

@interface ClearBrowsingDataController : NSWindowController {
 @private
  Profile* profile_;  // Weak, owned by browser.
  // If non-null means there is a removal in progress. Member used mainly for
  // automated tests. The remove deletes itself when it's done, so this is a
  // weak reference.
  BrowsingDataRemover* remover_;
  scoped_ptr<ClearBrowsingObserver> observer_;
  BOOL isClearing_;  // YES while clearing data is ongoing.

  // Values for checkboxes, kept in sync with bindings. These values get
  // persisted into prefs if the user accepts the dialog.
  BOOL clearBrowsingHistory_;
  BOOL clearDownloadHistory_;
  BOOL emptyCache_;
  BOOL deleteCookies_;
  BOOL clearSavedPasswords_;
  BOOL clearFormData_;
  NSInteger timePeriod_;
}

// Show the clear browsing data window.  Do not use |-initWithProfile:|,
// go through this instead so we don't end up with multiple instances.
+ (void)showClearBrowsingDialogForProfile:(Profile*)profile;
+ (ClearBrowsingDataController*)controllerForProfile:(Profile*)profile;

// Run the dialog with an application-modal event loop. If the user accepts,
// performs the deletion of the selected browsing data. The values of the
// checkboxes will be persisted into prefs for next time.
- (void)runModalDialog;

// IBActions for the dialog buttons
- (IBAction)clearData:(id)sender;
- (IBAction)cancel:(id)sender;

// Properties for bindings
@property BOOL clearBrowsingHistory;
@property BOOL clearDownloadHistory;
@property BOOL emptyCache;
@property BOOL deleteCookies;
@property BOOL clearSavedPasswords;
@property BOOL clearFormData;
@property NSInteger timePeriod;
@property BOOL isClearing;

@end


@interface ClearBrowsingDataController (ExposedForUnitTests)
// Create the controller with the given profile (which must not be NULL).
- (id)initWithProfile:(Profile*)profile;
@property (readonly) int removeMask;
- (void)persistToPrefs;
- (void)closeDialog;
- (void)dataRemoverDidFinish;
@end

#endif  // CHROME_BROWSER_COCOA_CLEAR_BROWSING_DATA_CONTROLLER_
