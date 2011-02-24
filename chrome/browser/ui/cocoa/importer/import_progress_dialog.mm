// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/importer/import_progress_dialog.h"

#include "base/logging.h"
#include "base/mac/mac_util.h"
#include "base/message_loop.h"
#import "base/scoped_nsobject.h"
#import "base/sys_string_conversions.h"
#include "base/utf_string_conversions.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/l10n_util_mac.h"

namespace {

// Convert ImportItem enum into the name of the ImportProgressDialogController
// property corresponding to the text for that item, this makes the code to
// change the values for said properties much more readable.
NSString* keyForImportItem(importer::ImportItem item) {
  switch(item) {
    case importer::HISTORY:
      return @"historyStatusText";
    case importer::FAVORITES:
      return @"favoritesStatusText";
    case importer::PASSWORDS:
      return @"savedPasswordStatusText";
    case importer::SEARCH_ENGINES:
      return @"searchStatusText";
    default:
      DCHECK(false);
      break;
  }
  return nil;
}

}  // namespace

@implementation ImportProgressDialogController

@synthesize explanatoryText = explanatory_text_;
@synthesize favoritesStatusText = favorites_status_text_;
@synthesize searchStatusText = search_status_text_;
@synthesize savedPasswordStatusText = saved_password_status_text_;
@synthesize historyStatusText = history_status_text_;

@synthesize favoritesImportEnabled = favorites_import_enabled_;
@synthesize searchImportEnabled = search_import_enabled_;
@synthesize passwordImportEnabled = password_import_enabled_;
@synthesize historyImportEnabled = history_import_enabled_;

- (id)initWithImporterHost:(ImporterHost*)host
               browserName:(string16)browserName
                  observer:(ImportObserver*)observer
              itemsEnabled:(int16)items {
  NSString* nib_path =
      [base::mac::MainAppBundle() pathForResource:@"ImportProgressDialog"
                                          ofType:@"nib"];
  self = [super initWithWindowNibPath:nib_path owner:self];
  if (self != nil) {
    importer_host_ = host;
    observer_ = observer;
    import_host_observer_bridge_.reset(new ImporterObserverBridge(self));
    importer_host_->SetObserver(import_host_observer_bridge_.get());

    string16 productName = l10n_util::GetStringUTF16(IDS_PRODUCT_NAME);
    NSString* explanatory_text = l10n_util::GetNSStringF(
        IDS_IMPORT_PROGRESS_EXPLANATORY_TEXT_MAC,
        productName,
        browserName);
    [self setExplanatoryText:explanatory_text];

    progress_text_ =
        [l10n_util::GetNSStringWithFixup(IDS_IMPORT_IMPORTING_PROGRESS_TEXT_MAC)
        retain];
    done_text_ =
        [l10n_util::GetNSStringWithFixup(IDS_IMPORT_IMPORTING_DONE_TEXT_MAC)
         retain];

    // Enable/disable item titles.
    NSColor* disabled = [NSColor disabledControlTextColor];
    NSColor* active = [NSColor textColor];
    [self setFavoritesImportEnabled:items & importer::FAVORITES ? active :
        disabled];
    [self setSearchImportEnabled:items & importer::SEARCH_ENGINES ? active :
        disabled];
    [self setPasswordImportEnabled:items & importer::PASSWORDS ? active :
        disabled];
    [self setHistoryImportEnabled:items & importer::HISTORY ? active :
        disabled];
  }
  return self;
}

- (void)dealloc {
  [explanatory_text_ release];
  [favorites_status_text_ release];
  [search_status_text_ release];
  [saved_password_status_text_ release];
  [history_status_text_ release];

  [favorites_import_enabled_ release];
  [search_import_enabled_ release];
  [password_import_enabled_ release];
  [history_import_enabled_ release];

  [progress_text_ release];
  [done_text_ release];

  [super dealloc];
}

- (IBAction)showWindow:(id)sender {
  NSWindow* win = [self window];
  [win center];
  [super showWindow:nil];
}

- (void)closeDialog {
  if ([[self window] isVisible]) {
    [[self window] close];
  }
}

- (IBAction)cancel:(id)sender {
  // The ImporterHost will notify import_host_observer_bridge_ that import has
  // ended, which will trigger the ImportEnded method, in which this object is
  // released.
  importer_host_->Cancel();
}

- (void)ImportItemStarted:(importer::ImportItem)item {
  [self setValue:progress_text_ forKey:keyForImportItem(item)];
}

- (void)ImportItemEnded:(importer::ImportItem)item {
  [self setValue:done_text_ forKey:keyForImportItem(item)];
}

- (void)ImportEnded {
  importer_host_->SetObserver(NULL);
  if (observer_)
    observer_->ImportComplete();
  [self closeDialog];
  [self release];

  // Break out of modal event loop.
  [NSApp stopModal];
}

@end

void StartImportingWithUI(gfx::NativeWindow parent_window,
                          uint16 items,
                          ImporterHost* coordinator,
                          const importer::ProfileInfo& source_profile,
                          Profile* target_profile,
                          ImportObserver* observer,
                          bool first_run) {
  DCHECK(items != 0);

  // Retrieve name of browser we're importing from and do a little dance to
  // convert wstring -> string16.
  string16 import_browser_name = WideToUTF16Hack(source_profile.description);

  // progress_dialog_ is responsible for deleting itself.
  ImportProgressDialogController* progress_dialog_ =
      [[ImportProgressDialogController alloc]
          initWithImporterHost:coordinator
                  browserName:import_browser_name
                      observer:observer
                  itemsEnabled:items];
   // Call is async.
  coordinator->StartImportSettings(source_profile, target_profile, items,
                                   new ProfileWriter(target_profile),
                                   first_run);

  // Display the window while spinning a message loop.
  // For details on why we need a modal message loop see http://crbug.com/19169
  NSWindow* progress_window = [progress_dialog_ window];
  NSModalSession session = [NSApp beginModalSessionForWindow:progress_window];
  [progress_dialog_ showWindow:nil];
  while (true) {
    if ([NSApp runModalSession:session] != NSRunContinuesResponse)
        break;
    MessageLoop::current()->RunAllPending();
  }
  [NSApp endModalSession:session];
}

ImporterObserverBridge::ImporterObserverBridge(
    ImportProgressDialogController* owner)
    : owner_(owner) {}

ImporterObserverBridge::~ImporterObserverBridge() {}

void ImporterObserverBridge::ImportItemStarted(importer::ImportItem item) {
  [owner_ ImportItemStarted:item];
}

void ImporterObserverBridge::ImportItemEnded(importer::ImportItem item) {
  [owner_ ImportItemEnded:item];
}

void ImporterObserverBridge::ImportStarted() {
  // Not needed for out of process import.
}

void ImporterObserverBridge::ImportEnded() {
  [owner_ ImportEnded];
}
