// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/first_run_dialog.h"

#include "app/l10n_util_mac.h"
#include "base/logging.h"
#include "base/mac_util.h"
#import "base/scoped_nsobject.h"
#include "grit/locale_settings.h"

@implementation FirstRunDialogController

@synthesize userDidCancel = user_did_cancel_;
@synthesize statsEnabled = stats_enabled_;
@synthesize makeDefaultBrowser = make_default_browser_;
@synthesize importBookmarks = import_bookmarks_;
@synthesize browserImportSelectedIndex = browser_import_selected_index_;
@synthesize browserImportList = browser_import_list_;
@synthesize browserImportListHidden = browser_import_list_hidden_;

- (id)init {
  NSString* nibpath =
      [mac_util::MainAppBundle() pathForResource:@"FirstRunDialog"
                                          ofType:@"nib"];
  self = [super initWithWindowNibPath:nibpath owner:self];
  if (self != nil) {
    // Bound to the dialog checkbox, default to true.
    stats_enabled_ = YES;
    import_bookmarks_ = YES;

#if !defined(GOOGLE_CHROME_BUILD)
    // In Chromium builds all stats reporting is disabled so there's no reason
    // to display the checkbox - the setting is always OFF.
    usage_stats_checkbox_hidden_ = YES;
#endif // !GOOGLE_CHROME_BUILD
  }
  return self;
}

- (void)dealloc {
  [browser_import_list_ release];
  [super dealloc];
}

- (IBAction)showWindow:(id)sender {
  // Neat weirdness in the below code - the Application menu stays enabled
  // while the window is open but selecting items from it (e.g. Quit) has
  // no effect.  I'm guessing that this is an artifact of us being a
  // background-only application at this stage and displaying a modal
  // window.

  // Display dialog.
  NSWindow* win = [self window];
  [win center];
  [NSApp runModalForWindow:win];
}

- (void)closeDialog {
  [[self window] close];
  [NSApp stopModal];
}

- (IBAction)ok:(id)sender {
  [self closeDialog];
}

- (IBAction)cancel:(id)sender {
  [self closeDialog];
  [self setUserDidCancel:YES];
}

- (IBAction)learnMore:(id)sender {
  NSString* urlStr = l10n_util::GetNSString(IDS_LEARN_MORE_REPORTING_URL);
  NSURL* learnMoreUrl = [NSURL URLWithString:urlStr];
  [[NSWorkspace sharedWorkspace] openURL:learnMoreUrl];
}

@end
