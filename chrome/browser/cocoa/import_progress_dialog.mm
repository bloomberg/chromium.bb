// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/import_progress_dialog.h"

#include "app/l10n_util_mac.h"
#include "base/logging.h"
#include "base/message_loop.h"
#import "base/scoped_nsobject.h"
#import "base/sys_string_conversions.h"
#include "grit/generated_resources.h"

namespace {

// Convert ImportItem enum into the name of the ImportProgressDialogController
// property corresponding to the text for that item, this makes the code to
// change the values for said properties much more readable.
NSString* keyForImportItem(ImportItem item) {
  switch(item) {
    case HISTORY:
      return @"historyStatusText";
    case FAVORITES:
      return @"favoritesStatusText";
    case PASSWORDS:
      return @"savedPasswordStatusText";
    case SEARCH_ENGINES:
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
              itemsEnabled:(int16)items; {
  self = [super initWithWindowNibName:@"ImportProgressDialog"];
  if (self != nil) {
    importer_host_ = host;
    observer_ = observer;
    import_host_observer_bridge_.reset(new ImporterObserverBridge(self));
    importer_host_->SetObserver(import_host_observer_bridge_.get());

    NSString* explanatory_text = l10n_util::GetNSStringF(
        IDS_IMPORT_PROGRESS_EXPLANATORY_TEXT_MAC, browserName);
    [self setExplanatoryText:explanatory_text];

    progress_text_ =
        [l10n_util::GetNSString(IDS_IMPORT_IMPORTING_PROGRESS_TEXT_MAC) retain];
    done_text_ =
        [l10n_util::GetNSString(IDS_IMPORT_IMPORTING_DONE_TEXT_MAC) retain];

    // Enable/disable item titles.
    NSColor* disabled = [NSColor disabledControlTextColor];
    NSColor* active = [NSColor textColor];
    [self setFavoritesImportEnabled:items & FAVORITES ? active : disabled];
    [self setSearchImportEnabled:items & SEARCH_ENGINES ? active : disabled];
    [self setPasswordImportEnabled:items & PASSWORDS ? active : disabled];
    [self setHistoryImportEnabled:items & HISTORY ? active : disabled];
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
  [self closeDialog];
  if (importing_) {
    importer_host_->Cancel();
  } else {
    [self release];
  }
}
- (void)ImportItemStarted:(ImportItem)item {
  [self setValue:progress_text_ forKey:keyForImportItem(item)];
}

- (void)ImportItemEnded:(ImportItem)item {
  [self setValue:done_text_ forKey:keyForImportItem(item)];
}

- (void)ImportStarted {
  importing_ = YES;
}

- (void)ImportEnded {
  importing_ = NO;
  importer_host_->SetObserver(NULL);
  if (observer_)
    observer_->ImportComplete();
  [self closeDialog];
  [self release];

  MessageLoop::current()->Quit();
}

@end

void StartImportingWithUI(gfx::NativeWindow parent_window,
                          int16 items,
                          ImporterHost* coordinator,
                          const ProfileInfo& source_profile,
                          Profile* target_profile,
                          ImportObserver* observer,
                          bool first_run) {
  DCHECK(items != 0);

  // Retrieve name of browser we're importing from and do a little dance to
  // convert wstring -> string16.
  using base::SysCFStringRefToUTF16;
  using base::SysWideToCFStringRef;
  string16 import_browser_name =
      SysCFStringRefToUTF16(SysWideToCFStringRef(source_profile.description));

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

  [progress_dialog_ showWindow:nil];
  MessageLoop::current()->Run();
}
