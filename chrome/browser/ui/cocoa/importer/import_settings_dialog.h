// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_IMPORTER_IMPORT_SETTINGS_DIALOG_H_
#define CHROME_BROWSER_UI_COCOA_IMPORTER_IMPORT_SETTINGS_DIALOG_H_
#pragma once

#import <Cocoa/Cocoa.h>

#include "base/scoped_nsobject.h"
#include "chrome/browser/importer/importer.h"

class Profile;
class ImporterListObserverBridge;

// Controller for the Import Bookmarks and Settings dialog.  This controller
// automatically autoreleases itself when its associated dialog is dismissed.
@interface ImportSettingsDialogController : NSWindowController {
 @private
  NSWindow* parentWindow_;  // weak
  Profile* profile_;  // weak
  scoped_refptr<ImporterList> importerList_;
  scoped_ptr<ImporterListObserverBridge> importerListObserver_;
  scoped_nsobject<NSArray> sourceBrowsersList_;
  NSUInteger sourceBrowserIndex_;
  // The following are all bound via the properties below.
  BOOL importHistory_;
  BOOL importFavorites_;
  BOOL importPasswords_;
  BOOL importSearchEngines_;
  BOOL historyAvailable_;
  BOOL favoritesAvailable_;
  BOOL passwordsAvailable_;
  BOOL searchEnginesAvailable_;
  IBOutlet NSPopUpButton* sourceProfilePopUpButton_;
}

// Show the import settings window.  Window is displayed as an app modal dialog.
// If the dialog is already being displayed, this method whill return with
// no error.
+ (void)showImportSettingsDialogForProfile:(Profile*)profile;

// Called when the "Import" button is pressed.
- (IBAction)ok:(id)sender;

// Cancel button calls this.
- (IBAction)cancel:(id)sender;

// An array of ImportSettingsProfiles, provide the list of browser profiles
// available for importing. Bound to the Browser List array controller.
- (NSArray*)sourceBrowsersList;

// Called when source profiles have been loaded.
- (void)sourceProfilesLoaded;

// Properties for bindings.
@property(assign, nonatomic) NSUInteger sourceBrowserIndex;
@property(assign, readonly, nonatomic) BOOL importSomething;
// Bindings for the value of the import checkboxes.
@property(assign, nonatomic) BOOL importHistory;
@property(assign, nonatomic) BOOL importFavorites;
@property(assign, nonatomic) BOOL importPasswords;
@property(assign, nonatomic) BOOL importSearchEngines;
// Bindings for enabling/disabling the checkboxes.
@property(assign, readonly, nonatomic) BOOL historyAvailable;
@property(assign, readonly, nonatomic) BOOL favoritesAvailable;
@property(assign, readonly, nonatomic) BOOL passwordsAvailable;
@property(assign, readonly, nonatomic) BOOL searchEnginesAvailable;

@end

@interface ImportSettingsDialogController (TestingAPI)

// Initialize by providing an array of source profile dictionaries. Exposed for
// unit testing but also called by -[initWithProfile:].
- (id)initWithSourceProfiles:(NSArray*)profiles;

// Return selected services to import as mapped by the ImportItem enum.
- (uint16)servicesToImport;

@end

// Utility class used as array elements for sourceBrowsersList, above.
@interface ImportSettingsProfile : NSObject {
 @private
  NSString* browserName_;
  uint16 services_;  // Services as defined by enum ImportItem.
}

// Convenience creator. |services| is a bitfield of enum ImportItems.
+ (id)importSettingsProfileWithBrowserName:(NSString*)browserName
                                  services:(uint16)services;

// Designated initializer. |services| is a bitfield of enum ImportItems.
- (id)initWithBrowserName:(NSString*)browserName
                 services:(uint16)services;  // Bitfield of enum ImportItems.

@property(copy, nonatomic) NSString* browserName;
@property(assign, nonatomic) uint16 services;  // Bitfield of enum ImportItems.

@end

#endif  // CHROME_BROWSER_UI_COCOA_IMPORTER_IMPORT_SETTINGS_DIALOG_H_
