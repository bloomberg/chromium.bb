// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_IMPORTER_IMPORT_PROGRESS_DIALOG_H_
#define CHROME_BROWSER_IMPORTER_IMPORT_PROGRESS_DIALOG_H_
#pragma once

#import <Cocoa/Cocoa.h>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/importer/importer_data_types.h"
#include "chrome/browser/importer/importer_progress_observer.h"

class ImporterHost;
class ImporterObserver;
class ImporterObserverBridge;

// Class that acts as a controller for the dialog that shows progress for an
// import operation.
// Lifetime: This object is responsible for deleting itself.
@interface ImportProgressDialogController : NSWindowController {
  scoped_ptr<ImporterObserverBridge> import_host_observer_bridge_;
  ImporterHost* importer_host_;  // (weak)
  ImporterObserver* observer_;   // (weak)

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

@property(nonatomic, retain) NSString* explanatoryText;
@property(nonatomic, retain) NSString* favoritesStatusText;
@property(nonatomic, retain) NSString* searchStatusText;
@property(nonatomic, retain) NSString* savedPasswordStatusText;
@property(nonatomic, retain) NSString* historyStatusText;

@property(nonatomic, retain) NSColor* favoritesImportEnabled;
@property(nonatomic, retain) NSColor* searchImportEnabled;
@property(nonatomic, retain) NSColor* passwordImportEnabled;
@property(nonatomic, retain) NSColor* historyImportEnabled;

// Cancel button calls this.
- (IBAction)cancel:(id)sender;

// Closes the dialog.
- (void)closeDialog;

// Methods called by importer_host via ImporterObserverBridge.
- (void)ImportItemStarted:(importer::ImportItem)item;
- (void)ImportItemEnded:(importer::ImportItem)item;
- (void)ImportEnded;

@end

// C++ -> objc bridge for import status notifications.
class ImporterObserverBridge : public importer::ImporterProgressObserver {
 public:
  ImporterObserverBridge(ImportProgressDialogController* owner);
  virtual ~ImporterObserverBridge();

 private:
  // importer::ImporterProgressObserver:
  virtual void ImportStarted() OVERRIDE;
  virtual void ImportItemStarted(importer::ImportItem item) OVERRIDE;
  virtual void ImportItemEnded(importer::ImportItem item) OVERRIDE;
  virtual void ImportEnded() OVERRIDE;

  ImportProgressDialogController* owner_;

  DISALLOW_COPY_AND_ASSIGN(ImporterObserverBridge);
};

#endif  // CHROME_BROWSER_IMPORTER_IMPORT_PROGRESS_DIALOG_H_
