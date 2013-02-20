// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "chrome/browser/speech/speech_recognition_bubble.h"

#import "base/memory/scoped_nsobject.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/cocoa/browser_window_cocoa.h"
#include "chrome/browser/ui/cocoa/browser_window_controller.h"
#include "chrome/browser/ui/cocoa/location_bar/location_bar_view_mac.h"
#import "chrome/browser/ui/cocoa/speech_recognition_window_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"
#include "ui/gfx/image/image_skia_util_mac.h"

using content::WebContents;

namespace {

// A class to bridge between the speech recognition C++ code and the Objective-C
// bubble implementation. See chrome/browser/speech/speech_recognition_bubble.h
// for more information on how this gets used.
class SpeechRecognitionBubbleImpl : public SpeechRecognitionBubbleBase {
 public:
  SpeechRecognitionBubbleImpl(WebContents* web_contents,
                              Delegate* delegate,
                              const gfx::Rect& element_rect);
  virtual ~SpeechRecognitionBubbleImpl();
  virtual void Show() OVERRIDE;
  virtual void Hide() OVERRIDE;
  virtual void UpdateLayout() OVERRIDE;
  virtual void UpdateImage() OVERRIDE;

 private:
  scoped_nsobject<SpeechRecognitionWindowController> window_;
  Delegate* delegate_;
  gfx::Rect element_rect_;
};

SpeechRecognitionBubbleImpl::SpeechRecognitionBubbleImpl(
    WebContents* web_contents,
    Delegate* delegate,
    const gfx::Rect& element_rect)
    : SpeechRecognitionBubbleBase(web_contents),
      delegate_(delegate),
      element_rect_(element_rect) {
}

SpeechRecognitionBubbleImpl::~SpeechRecognitionBubbleImpl() {
  if (window_.get())
    [window_.get() close];
}

void SpeechRecognitionBubbleImpl::UpdateImage() {
  if (window_.get())
    [window_.get() setImage:gfx::NSImageFromImageSkia(icon_image())];
}

void SpeechRecognitionBubbleImpl::Show() {
  if (window_.get()) {
    [window_.get() show];
    return;
  }

  // Find the screen coordinates for the given tab and position the bubble's
  // arrow anchor point inside that to point at the bottom-left of the html
  // input element rect if the position is valid, otherwise point it towards
  // the page icon in the omnibox.
  gfx::NativeView view = GetWebContents()->GetView()->GetNativeView();
  NSWindow* parent_window = [view window];
  NSRect tab_bounds = [view bounds];
  int anchor_x = tab_bounds.origin.x + element_rect_.x() +
                 element_rect_.width() - kBubbleTargetOffsetX;
  int anchor_y = tab_bounds.origin.y + tab_bounds.size.height -
                 element_rect_.y() - element_rect_.height();

  NSPoint anchor = NSMakePoint(anchor_x, anchor_y);
  if (NSPointInRect(anchor, tab_bounds)) {
    // Good, convert to window coordinates.
    anchor = [view convertPoint:anchor toView:nil];
  } else {
    LocationBarViewMac* locationBar =
        [[parent_window windowController] locationBarBridge];

    if (locationBar) {
      anchor = locationBar->GetPageInfoBubblePoint();
    } else {
      // This is very rare, but possible. Just use the top-left corner.
      // See http://crbug.com/119237
      anchor = NSMakePoint(NSMinX(tab_bounds), NSMaxY(tab_bounds));
      anchor = [view convertPoint:anchor toView:nil];
    }
  }

  anchor = [parent_window convertBaseToScreen:anchor];

  window_.reset([[SpeechRecognitionWindowController alloc]
      initWithParentWindow:parent_window
                  delegate:delegate_
                anchoredAt:anchor]);

  UpdateLayout();
  [window_.get() show];
}

void SpeechRecognitionBubbleImpl::Hide() {
  if (!window_.get())
    return;

  [window_.get() close];
  window_.reset();
}

void SpeechRecognitionBubbleImpl::UpdateLayout() {
  if (!window_.get())
    return;

  [window_.get() updateLayout:display_mode()
                  messageText:message_text()
                    iconImage:gfx::NSImageFromImageSkia(icon_image())];
}

}  // namespace

SpeechRecognitionBubble* SpeechRecognitionBubble::CreateNativeBubble(
    WebContents* web_contents,
    Delegate* delegate,
    const gfx::Rect& element_rect) {
  return new SpeechRecognitionBubbleImpl(web_contents, delegate, element_rect);
}
