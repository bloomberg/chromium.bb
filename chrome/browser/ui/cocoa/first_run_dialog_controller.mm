// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/first_run_dialog_controller.h"

#include "base/mac/scoped_nsobject.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/browser/ui/cocoa/key_equivalent_constants.h"
#include "chrome/browser/ui/cocoa/l10n_util.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/cocoa/controls/button_utils.h"
#include "ui/base/cocoa/controls/textfield_utils.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/l10n_util_mac.h"

namespace {

// Return the internationalized message |message_id|, with the product name
// substituted in for $1.
NSString* NSStringWithProductName(int message_id) {
  return l10n_util::GetNSStringF(message_id,
                                 l10n_util::GetStringUTF16(IDS_PRODUCT_NAME));
}

void MoveViewsDown(NSArray* views, CGFloat distance) {
  for (NSView* view : views) {
    NSRect frame = view.frame;
    frame.origin.y -= distance;
    [view setFrame:frame];
  }
}

}  // namespace

@implementation FirstRunDialogViewController {
  // These are owned by the NSView hierarchy:
  NSButton* defaultBrowserCheckbox_;
  NSButton* statsCheckbox_;

  // This is owned by NSViewController:
  NSView* view_;

  BOOL statsCheckboxInitiallyChecked_;
  BOOL defaultBrowserCheckboxVisible_;
}

- (instancetype)initWithStatsCheckboxInitiallyChecked:(BOOL)checked
                        defaultBrowserCheckboxVisible:(BOOL)visible {
  if ((self = [super init])) {
    statsCheckboxInitiallyChecked_ = checked;
    defaultBrowserCheckboxVisible_ = visible;
  }
  return self;
}

- (void)loadView {
  // Frame constants in this method were taken directly from the now-deleted
  // chrome/app/nibs/FirstRunDialog.xib.
  NSBox* topBox =
      [[[NSBox alloc] initWithFrame:NSMakeRect(0, 158, 480, 55)] autorelease];
  [topBox setFillColor:[NSColor whiteColor]];
  [topBox setBoxType:NSBoxCustom];
  [topBox setBorderType:NSNoBorder];
  [topBox setContentViewMargins:NSZeroSize];

  NSTextField* completionLabel = [TextFieldUtils
      labelWithString:NSStringWithProductName(
                          IDS_FIRSTRUN_DLG_MAC_COMPLETE_INSTALLATION_LABEL)];
  [completionLabel setFrame:NSMakeRect(13, 25, 390, 17)];

  NSImageView* logoImage = [[[NSImageView alloc]
      initWithFrame:NSMakeRect(408, -25, 96, 96)] autorelease];
  [logoImage setImage:[NSImage imageNamed:NSImageNameApplicationIcon]];
  [logoImage setRefusesFirstResponder:YES];
  [logoImage setEditable:NO];

  defaultBrowserCheckbox_ = [ButtonUtils
      checkboxWithTitle:l10n_util::GetNSString(
                            IDS_FIRSTRUN_DLG_MAC_SET_DEFAULT_BROWSER_LABEL)];
  [defaultBrowserCheckbox_ setFrame:NSMakeRect(45, 126, 528, 18)];
  if (!defaultBrowserCheckboxVisible_)
    [defaultBrowserCheckbox_ setHidden:YES];

  statsCheckbox_ = [ButtonUtils
      checkboxWithTitle:
          NSStringWithProductName(
              IDS_FIRSTRUN_DLG_MAC_OPTIONS_SEND_USAGE_STATS_LABEL)];
  [statsCheckbox_ setFrame:NSMakeRect(45, 101, 389, 19)];
  if (statsCheckboxInitiallyChecked_)
    [statsCheckbox_ setNextState];

  NSButton* startChromeButton =
      [ButtonUtils buttonWithTitle:NSStringWithProductName(
                                       IDS_FIRSTRUN_DLG_MAC_START_CHROME_BUTTON)
                            action:@selector(ok:)
                            target:self];
  [startChromeButton setFrame:NSMakeRect(161, 12, 306, 32)];
  [startChromeButton setKeyEquivalent:kKeyEquivalentReturn];

  NSButton* learnMoreLink =
      [ButtonUtils linkWithTitle:l10n_util::GetNSString(IDS_LEARN_MORE)
                          action:@selector(learnMore:)
                          target:self];
  [learnMoreLink setFrame:NSMakeRect(60, 76, 359, 19)];

  NSBox* topSeparator =
      [[[NSBox alloc] initWithFrame:NSMakeRect(0, 155, 480, 5)] autorelease];
  [topSeparator setBoxType:NSBoxSeparator];

  NSBox* bottomSeparator =
      [[[NSBox alloc] initWithFrame:NSMakeRect(0, 55, 480, 5)] autorelease];
  [bottomSeparator setBoxType:NSBoxSeparator];

  [topBox addSubview:completionLabel];
  [topBox addSubview:logoImage];

  base::scoped_nsobject<NSView> content_view(
      [[NSView alloc] initWithFrame:NSMakeRect(0, 0, 480, 209)]);
  self.view = content_view.get();
  [self.view addSubview:topBox];
  [self.view addSubview:topSeparator];
  [self.view addSubview:defaultBrowserCheckbox_];
  [self.view addSubview:statsCheckbox_];
  [self.view addSubview:learnMoreLink];
  [self.view addSubview:bottomSeparator];
  [self.view addSubview:startChromeButton];

  // Now that the content view is constructed, fix the layout: since this view
  // isn't using autolayout, if the widths of some of the subviews change
  // because of localization, they need to be resized and perhaps repositioned,
  // which is done here by |VerticallyReflowGroup()|.
  CGFloat oldWidth = NSWidth([startChromeButton frame]);
  cocoa_l10n_util::VerticallyReflowGroup(
      @[ defaultBrowserCheckbox_, statsCheckbox_, learnMoreLink ]);

  // The "Start Chrome" button needs to be sized to fit the localized string
  // inside it, but it should still be at the right-most edge of the dialog, so
  // any width added or subtracted by |sizeToFit| is added to its x coord, which
  // keeps its right edge where it was.
  [startChromeButton sizeToFit];
  NSRect frame = [startChromeButton frame];
  frame.origin.x += oldWidth - NSWidth([startChromeButton frame]);
  [startChromeButton setFrame:frame];

  // Lastly, if the default browser checkbox is actually invisible, move the
  // views above it downward so that there's not a big open space in the content
  // view, and resize the content view itself so there isn't extra space.
  if (!defaultBrowserCheckboxVisible_) {
    CGFloat delta = NSHeight([defaultBrowserCheckbox_ frame]);
    MoveViewsDown(@[ topBox, topSeparator ], delta);
    NSRect frame = [self.view frame];
    frame.size.height -= delta;
    [self.view setAutoresizesSubviews:NO];
    [self.view setFrame:frame];
    [self.view setAutoresizesSubviews:YES];
  }
}

- (NSString*)windowTitle {
  return NSStringWithProductName(IDS_FIRSTRUN_DLG_MAC_WINDOW_TITLE);
}

- (BOOL)isStatsReportingEnabled {
  return [statsCheckbox_ state] == NSOnState;
}

- (BOOL)isMakeDefaultBrowserEnabled {
  return [defaultBrowserCheckbox_ state] == NSOnState;
}

- (void)ok:(id)sender {
  [[[self view] window] close];
  [NSApp stopModal];
}

- (void)learnMore:(id)sender {
  NSString* urlStr = base::SysUTF8ToNSString(chrome::kLearnMoreReportingURL);
  NSURL* learnMoreUrl = [NSURL URLWithString:urlStr];
  [[NSWorkspace sharedWorkspace] openURL:learnMoreUrl];
}

@end
