// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/browser/avatar_button_controller.h"

#include "base/strings/sys_string_conversions.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#import "chrome/browser/ui/cocoa/browser_window_controller.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/text_elider.h"

namespace {

NSString* GetElidedProfileName(const base::string16& name) {
  // Maximum characters the button can be before the text will get elided.
  const int kMaxCharactersToDisplay = 15;

  gfx::FontList font_list = ui::ResourceBundle::GetSharedInstance().GetFontList(
      ui::ResourceBundle::BaseFont);
  return base::SysUTF16ToNSString(gfx::ElideText(
      name,
      font_list,
      font_list.GetExpectedTextWidth(kMaxCharactersToDisplay),
      gfx::ELIDE_AT_END));
}

}  // namespace

@interface AvatarButtonController (Private)
- (void)updateAvatarButtonAndLayoutParent:(BOOL)layoutParent;
@end

@implementation AvatarButtonController

- (id)initWithBrowser:(Browser*)browser {
  if ((self = [super initWithBrowser:browser])) {
    button_.reset([[NSButton alloc] initWithFrame:NSZeroRect]);
    [self setView:button_];

    [button_ setBezelStyle:NSTexturedRoundedBezelStyle];
    [button_ setImage:ui::ResourceBundle::GetSharedInstance().
        GetNativeImageNamed(IDR_APP_DROPARROW).ToNSImage()];
    [button_ setImagePosition:NSImageRight];
    [button_ setAutoresizingMask:NSViewMinXMargin | NSViewMinYMargin];
    [button_ setTarget:self];
    [button_ setAction:@selector(buttonClicked:)];

    [self updateAvatarButtonAndLayoutParent:NO];
  }
  return self;
}

- (void)updateAvatarButtonAndLayoutParent:(BOOL)layoutParent {
  [button_ setTitle:GetElidedProfileName(
      profiles::GetActiveProfileDisplayName(browser_))];
  [button_ sizeToFit];

  // Resize the container.
  [[self view] setFrameSize:[button_ frame].size];
  [button_ setFrameOrigin:NSMakePoint(0, 0)];

  if (layoutParent) {
    // Because the width of the button might have changed, the parent browser
    // frame needs to recalculate the button bounds and redraw it.
    [[BrowserWindowController
        browserWindowControllerForWindow:browser_->window()->GetNativeWindow()]
        layoutSubviews];
  }
}

@end
