// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_IMPORT_PROGRESS_DIALOG_H_
#define CHROME_BROWSER_IMPORT_PROGRESS_DIALOG_H_

#import <Cocoa/Cocoa.h>

#include "chrome/browser/importer/importer.h"

class ImporterObserverBridge;

// Class that acts as a controller for the dialog that shows progress for an
// import operation.
// Lifetime: This object is responsible for deleting itself.
@interface ImportProgressDialogController : NSWindowController {
  scoped_ptr<ImporterObserverBridge> import_host_observer_bridge_;
  ImporterHost* importer_host_;  // (weak)
  ImportObserver* observer_;  // (weak)
  BOOL importing_;

  // Strings bound to static labels in the UI dialog.
  NSString* explanatory_text_;
  NSString* favorites_status_text_;
  NSString* search_status_text_;
  NSString* saved_password_status_text_;
  NSString* history_status_text_;

  // Bound to the color of the status text (this is the easiest way to disable
  // progress items that aren't supported by the current browser we're importing
  // from).
  NSColor* favorites_import_enabled_;
  NSColor* search_import_enabled_;
  NSColor* password_import_enabled_;
  NSColor* history_import_enabled_;

  // Placeholders for "Importing..." and "Done" text.
  NSString* progress_text_;
  NSString* done_text_;
}

// Cancel button calls this.
- (IBAction)cancel:(id)sender;

// Closes the dialog.
- (void)closeDialog;

// Methods called by importer_host via ImporterObserverBridge.
- (void)ImportItemStarted:(ImportItem)item;
- (void)ImportItemEnded:(ImportItem)item;
- (void)ImportStarted;
- (void)ImportEnded;

@property(retain) NSString* explanatoryText;
@property(retain) NSString* favoritesStatusText;
@property(retain) NSString* searchStatusText;
@property(retain) NSString* savedPasswordStatusText;
@property(retain) NSString* historyStatusText;

@property(retain) NSColor* favoritesImportEnabled;
@property(retain) NSColor* searchImportEnabled;
@property(retain) NSColor* passwordImportEnabled;
@property(retain) NSColor* historyImportEnabled;

@end

// C++ -> objc bridge for import status notifications.
class ImporterObserverBridge : public ImporterHost::Observer {
 public:
  ImporterObserverBridge(ImportProgressDialogController* owner)
      : owner_(owner) {}
  virtual ~ImporterObserverBridge() {}

  // Invoked when data for the specified item is about to be collected.
  virtual void ImportItemStarted(ImportItem item) {
    [owner_ ImportItemStarted:item];
  }

  // Invoked when data for the specified item has been collected from the
  // source profile and is now ready for further processing.
  virtual void ImportItemEnded(ImportItem item) {
    [owner_ ImportItemEnded:item];
  }

  // Invoked when the import begins.
  virtual void ImportStarted() {
    [owner_ ImportStarted];
  }

  // Invoked when the source profile has been imported.
  virtual void ImportEnded() {
    [owner_ ImportEnded];
  }

 private:
  ImportProgressDialogController* owner_;

  DISALLOW_COPY_AND_ASSIGN(ImporterObserverBridge);
};

#endif  // CHROME_BROWSER_IMPORT_PROGRESS_DIALOG_H_
