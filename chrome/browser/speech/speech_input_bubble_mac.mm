// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "chrome/browser/speech/speech_input_bubble.h"

#import "base/scoped_nsobject.h"
#import "chrome/browser/ui/cocoa/speech_input_window_controller.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/browser/tab_contents/tab_contents_view.h"
#include "skia/ext/skia_utils_mac.h"

namespace {

// A class to bridge between the speech recognition C++ code and the Objective-C
// bubble implementation. See chrome/browser/speech/speech_input_bubble.h for
// more information on how this gets used.
class SpeechInputBubbleImpl : public SpeechInputBubbleBase {
 public:
  SpeechInputBubbleImpl(TabContents* tab_contents,
                        Delegate* delegate,
                        const gfx::Rect& element_rect);
  virtual ~SpeechInputBubbleImpl();
  virtual void Show();
  virtual void Hide();
  virtual void UpdateLayout();
  virtual void SetImage(const SkBitmap& image);

 private:
  scoped_nsobject<SpeechInputWindowController> window_;
  Delegate* delegate_;
  gfx::Rect element_rect_;
};

SpeechInputBubbleImpl::SpeechInputBubbleImpl(TabContents* tab_contents,
                                             Delegate* delegate,
                                             const gfx::Rect& element_rect)
    : SpeechInputBubbleBase(tab_contents),
      delegate_(delegate),
      element_rect_(element_rect) {
}

SpeechInputBubbleImpl::~SpeechInputBubbleImpl() {
  if (window_.get())
    [window_.get() close];
}

void SpeechInputBubbleImpl::SetImage(const SkBitmap& image) {
  if (window_.get())
    [window_.get() setImage:gfx::SkBitmapToNSImage(image)];
}

void SpeechInputBubbleImpl::Show() {
  if (window_.get()) {
    [window_.get() show];
    return;
  }

  // Find the screen coordinates for the given tab and position the bubble's
  // arrow anchor point inside that to point at the bottom-left of the html
  // input element rect.
  gfx::NativeView view = tab_contents()->view()->GetNativeView();
  NSRect tab_bounds = [view bounds];
  int anchor_x = tab_bounds.origin.x + element_rect_.x() +
                 element_rect_.width() - kBubbleTargetOffsetX;
  int anchor_y = tab_bounds.origin.y + tab_bounds.size.height -
                 element_rect_.y() - element_rect_.height();
  NSPoint anchor = NSMakePoint(anchor_x, anchor_y);
  anchor = [view convertPoint:anchor toView:nil];
  anchor = [[view window] convertBaseToScreen:anchor];

  window_.reset([[SpeechInputWindowController alloc]
      initWithParentWindow:tab_contents()->view()->GetTopLevelNativeWindow()
                  delegate:delegate_
                anchoredAt:anchor]);

  UpdateLayout();
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
                  messageText:message_text()];
}

}  // namespace

SpeechInputBubble* SpeechInputBubble::CreateNativeBubble(
    TabContents* tab_contents,
    Delegate* delegate,
    const gfx::Rect& element_rect) {
  return new SpeechInputBubbleImpl(tab_contents, delegate, element_rect);
}

