// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/save_password_infobar_delegate.h"
#include "chrome/browser/ui/chrome_style.h"
#include "chrome/browser/ui/cocoa/infobars/confirm_infobar_controller.h"
#include "chrome/browser/ui/cocoa/infobars/infobar_cocoa.h"
#include "components/infobars/core/infobar.h"
#include "skia/ext/skia_utils_mac.h"
#include "ui/base/cocoa/controls/hyperlink_text_view.h"

@interface SavePasswordInfobarController : ConfirmInfoBarController
@end

@implementation SavePasswordInfobarController

- (void)addAdditionalControls {
  [super addAdditionalControls];
  // Set the link inside the message.
  SavePasswordInfoBarDelegate* delegate =
      static_cast<SavePasswordInfoBarDelegate*>([self delegate]);
  gfx::Range linkRange = delegate->message_link_range();
  if (!linkRange.is_empty()) {
    NSColor* linkColor =
        gfx::SkColorToCalibratedNSColor(chrome_style::GetLinkColor());
    HyperlinkTextView* view = (HyperlinkTextView*)label_.get();
    [view addLinkRange:linkRange.ToNSRange()
               withURL:@"about:blank"  // using a link here is bad ui
             linkColor:linkColor];
  }
}

scoped_ptr<infobars::InfoBar> CreateSavePasswordInfoBar
    (scoped_ptr<SavePasswordInfoBarDelegate> delegate) {
  scoped_ptr<InfoBarCocoa> infobar(new InfoBarCocoa(delegate.Pass()));
  base::scoped_nsobject<SavePasswordInfobarController> controller(
      [[SavePasswordInfobarController alloc] initWithInfoBar:infobar.get()]);
  infobar->set_controller(controller);
  return infobar.Pass();
}

@end
