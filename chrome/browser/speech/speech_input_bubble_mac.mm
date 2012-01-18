// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "chrome/browser/speech/speech_input_bubble.h"

#import "base/memory/scoped_nsobject.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/cocoa/browser_window_cocoa.h"
#include "chrome/browser/ui/cocoa/browser_window_controller.h"
#include "chrome/browser/ui/cocoa/location_bar/location_bar_view_mac.h"
#import "chrome/browser/ui/cocoa/speech_input_window_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"
#include "skia/ext/skia_utils_mac.h"

using content::WebContents;

namespace {

// A class to bridge between the speech recognition C++ code and the Objective-C
// bubble implementation. See chrome/browser/speech/speech_input_bubble.h for
// more information on how this gets used.
class SpeechInputBubbleImpl : public SpeechInputBubbleBase {
 public:
  SpeechInputBubbleImpl(WebContents* web_contents,
                        Delegate* delegate,
                        const gfx::Rect& element_rect);
  virtual ~SpeechInputBubbleImpl();
  virtual void Show();
  virtual void Hide();
  virtual void UpdateLayout();
  virtual void UpdateImage();

 private:
  scoped_nsobject<SpeechInputWindowController> window_;
  Delegate* delegate_;
  gfx::Rect element_rect_;
};

SpeechInputBubbleImpl::SpeechInputBubbleImpl(WebContents* web_contents,
                                             Delegate* delegate,
                                             const gfx::Rect& element_rect)
    : SpeechInputBubbleBase(web_contents),
      delegate_(delegate),
      element_rect_(element_rect) {
}

SpeechInputBubbleImpl::~SpeechInputBubbleImpl() {
  if (window_.get())
    [window_.get() close];
}

void SpeechInputBubbleImpl::UpdateImage() {
  if (window_.get())
    [window_.get() setImage:gfx::SkBitmapToNSImage(icon_image())];
}

void SpeechInputBubbleImpl::Show() {
  if (window_.get()) {
    [window_.get() show];
    return;
  }

  // Find the screen coordinates for the given tab and position the bubble's
  // arrow anchor point inside that to point at the bottom-left of the html
  // input element rect if the position is valid, otherwise point it towards
  // the page icon in the omnibox.
  gfx::NativeView view = web_contents()->GetView()->GetNativeView();
  NSWindow* parentWindow = web_contents()->GetView()->GetTopLevelNativeWindow();
  NSRect tab_bounds = [view bounds];
  int anchor_x = tab_bounds.origin.x + element_rect_.x() +
                 element_rect_.width() - kBubbleTargetOffsetX;
  int anchor_y = tab_bounds.origin.y + tab_bounds.size.height -
                 element_rect_.y() - element_rect_.height();
  NSPoint anchor = NSZeroPoint;
  if (anchor_x < 0 || anchor_y < 0 ||
      anchor_x > NSWidth([parentWindow frame]) ||
      anchor_y > NSHeight([parentWindow frame])) {
    LocationBarViewMac* locationBar =
        [[parentWindow windowController] locationBarBridge];
    anchor = locationBar->GetPageInfoBubblePoint();
  } else {
    anchor = NSMakePoint(anchor_x, anchor_y);
  }
  anchor = [view convertPoint:anchor toView:nil];
  anchor = [[view window] convertBaseToScreen:anchor];

  window_.reset([[SpeechInputWindowController alloc]
      initWithParentWindow:parentWindow
                  delegate:delegate_
                anchoredAt:anchor]);

  UpdateLayout();
  [window_.get() show];
}

void SpeechInputBubbleImpl::Hide() {
  if (!window_.get())
    return;

  [window_.get() close];
  window_.reset();
}

void SpeechInputBubbleImpl::UpdateLayout() {
  if (!window_.get())
    return;

  [window_.get() updateLayout:display_mode()
                  messageText:message_text()
                    iconImage:gfx::SkBitmapToNSImage(icon_image())];
}

}  // namespace

SpeechInputBubble* SpeechInputBubble::CreateNativeBubble(
    WebContents* web_contents,
    Delegate* delegate,
    const gfx::Rect& element_rect) {
  return new SpeechInputBubbleImpl(web_contents, delegate, element_rect);
}

