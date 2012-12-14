// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/intents/web_intent_inline_service_view_controller.h"

#include "chrome/browser/ui/browser_finder.h"
#import "chrome/browser/ui/chrome_style.h"
#import "chrome/browser/ui/cocoa/constrained_window/constrained_window_control_utils.h"
#import "chrome/browser/ui/cocoa/flipped_view.h"
#import "chrome/browser/ui/cocoa/hyperlink_button_cell.h"
#import "chrome/browser/ui/cocoa/intents/web_intent_picker_cocoa.h"
#include "chrome/browser/ui/constrained_window.h"
#include "chrome/browser/ui/intents/web_intent_picker.h"
#include "chrome/browser/ui/intents/web_intent_picker_delegate.h"
#include "chrome/browser/ui/intents/web_intent_inline_disposition_delegate.h"
#include "content/public/browser/web_contents.h"
#include "grit/generated_resources.h"
#include "skia/ext/skia_utils_mac.h"
#include "ui/base/l10n/l10n_util_mac.h"

@implementation WebIntentInlineServiceViewController

- (id)initWithPicker:(WebIntentPickerCocoa*)picker {
  if ((self = [super init])) {
    picker_ = picker;

    scoped_nsobject<NSView> view(
        [[FlippedView alloc] initWithFrame:NSZeroRect]);
    [self setView:view];

    serviceIconImageView_.reset([[NSImageView alloc] initWithFrame:NSZeroRect]);
    [serviceIconImageView_ setImageFrameStyle:NSImageFrameNone];
    [[self view] addSubview:serviceIconImageView_];

    serviceNameTextField_.reset([constrained_window::CreateLabel() retain]);
    [[self view] addSubview:serviceNameTextField_];

    NSString *chooseString = l10n_util::GetNSStringWithFixup(
        IDS_INTENT_PICKER_USE_ALTERNATE_SERVICE);
    chooseServiceButton_.reset(
        [[HyperlinkButtonCell buttonWithString:chooseString] retain]);
    [[chooseServiceButton_ cell] setUnderlineOnHover:YES];
    [[chooseServiceButton_ cell] setTextColor:
        gfx::SkColorToCalibratedNSColor(chrome_style::GetLinkColor())];
    [chooseServiceButton_ sizeToFit];
    [[self view] addSubview:chooseServiceButton_];

    separator_.reset([[NSBox alloc] initWithFrame:NSZeroRect]);
    [separator_ setBoxType:NSBoxCustom];
    [separator_ setBorderColor:gfx::SkColorToCalibratedNSColor(
        chrome_style::GetSeparatorColor())];
    [[self view] addSubview:separator_];

    webContentView_.reset([[NSView alloc] initWithFrame:NSZeroRect]);
    [[self view] addSubview:webContentView_];
  }
  return self;
}

- (NSButton*)chooseServiceButton {
  return chooseServiceButton_;
}

- (content::WebContents*)webContents {
  return webContents_.get();
}

- (NSSize)minimumInlineWebViewSizeForFrame:(NSRect)frame {
  CGFloat height = NSHeight([serviceNameTextField_ frame]);
  height += WebIntentPicker::kHeaderSeparatorPaddingTop +
            WebIntentPicker::kHeaderSeparatorPaddingBottom;
  return NSMakeSize(NSWidth(frame), NSHeight(frame) - height);
}

- (void)setServiceName:(NSString*)serviceName {
  [serviceNameTextField_ setAttributedStringValue:
      constrained_window::GetAttributedLabelString(
          serviceName,
          chrome_style::kBoldTextFontStyle,
          NSLeftTextAlignment,
          NSLineBreakByTruncatingTail)];
  [serviceNameTextField_ sizeToFit];
}

- (void)setServiceIcon:(NSImage*)serviceIcon {
  [serviceIconImageView_ setImage:serviceIcon];
}

- (void)setServiceURL:(const GURL&)url {
  if (serviceURL_ == url)
    return;
  serviceURL_ = url;

  if (serviceURL_.is_empty()) {
    if (webContents_.get()) {
      [webContents_->GetNativeView() removeFromSuperview];
      webContents_.reset();
    }
    delegate_.reset();
    return;
  }

  Browser* browser = chrome::FindBrowserWithWebContents(
      picker_->web_contents());
  webContents_.reset(picker_->delegate()->CreateWebContentsForInlineDisposition(
          browser->profile(), url));
  delegate_.reset(new WebIntentInlineDispositionDelegate(
          picker_, webContents_.get(), browser));

  webContents_->GetController().LoadURL(url,
                                        content::Referrer(),
                                        content::PAGE_TRANSITION_AUTO_TOPLEVEL,
                                        std::string());
  [webContentView_ addSubview:webContents_->GetNativeView()];
}

- (void)setChooseServiceButtonHidden:(BOOL)isHidden {
  [chooseServiceButton_ setHidden:isHidden];
}

- (NSSize)minimumSizeForInnerWidth:(CGFloat)innerWidth {
  CGFloat height = NSHeight([serviceNameTextField_ frame]);
  height += WebIntentPicker::kHeaderSeparatorPaddingTop +
            WebIntentPicker::kHeaderSeparatorPaddingBottom;
  CGFloat width = innerWidth;
  if (webContents_.get()) {
    gfx::Size webSize(webContents_->GetPreferredSize());
    height += webSize.height();
    width = webSize.width();
  }
  return NSMakeSize(width, height);
}

- (void)layoutSubviewsWithinFrame:(NSRect)innerFrame {
  NSRect iconFrame;
  iconFrame.size.width = WebIntentPicker::kServiceIconWidth;
  iconFrame.size.height = WebIntentPicker::kServiceIconHeight;
  iconFrame.origin = innerFrame.origin;
  [serviceIconImageView_ setFrame:iconFrame];

  NSRect buttonRect = [chooseServiceButton_ frame];
  if ([chooseServiceButton_ isHidden])
    buttonRect.size.width = 0;
  buttonRect.origin.x = NSMaxX(innerFrame) - NSWidth(buttonRect) -
                        chrome_style::GetCloseButtonSize() -
                        WebIntentPicker::kIconTextPadding;
  buttonRect.origin.y = NSMinY(innerFrame);
  [chooseServiceButton_ setFrame:buttonRect];

  NSRect nameRect;
  nameRect.origin.x = NSMaxX(iconFrame) + WebIntentPicker::kIconTextPadding;
  nameRect.origin.y = NSMinY(innerFrame);
  nameRect.size.height = NSHeight([serviceNameTextField_ frame]);
  nameRect.size.width = NSMinX(buttonRect) - NSMinX(nameRect) -
                        WebIntentPicker::kIconTextPadding;
  [serviceNameTextField_ setFrame:nameRect];

  NSRect separatorFrame;
  separatorFrame.origin.x = 0;
  separatorFrame.origin.y = NSMaxY(nameRect) +
                            WebIntentPicker::kHeaderSeparatorPaddingTop;
  separatorFrame.size.width = NSWidth([[self view] bounds]);
  separatorFrame.size.height = 1.0;
  [separator_ setFrame:separatorFrame];

  if (webContents_.get()) {
    NSRect webFrame;
    webFrame.origin.y = NSMaxY(separatorFrame) +
                        WebIntentPicker::kHeaderSeparatorPaddingBottom;
    webFrame.origin.x = NSMinX(innerFrame);
    webFrame.size.width = NSWidth(innerFrame);
    webFrame.size.height = NSMaxY(innerFrame) - NSMinY(webFrame);
    [webContentView_ setFrame:webFrame];
    [webContents_->GetNativeView() setFrame:[webContentView_ bounds]];
  }
}

- (void)viewRemovedFromSuperview {
  [self setServiceURL:GURL::EmptyGURL()];
}

@end
