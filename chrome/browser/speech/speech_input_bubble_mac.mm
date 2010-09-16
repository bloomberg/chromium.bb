// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "chrome/browser/speech/speech_input_bubble.h"

#import "base/scoped_nsobject.h"
#import "chrome/browser/cocoa/speech_input_window_controller.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/tab_contents/tab_contents_view.h"

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
};

SpeechInputBubbleImpl::SpeechInputBubbleImpl(TabContents* tab_contents,
                                             Delegate* delegate,
                                             const gfx::Rect& element_rect) {
  // Find the screen coordinates for the given tab and position the bubble's
  // arrow anchor point inside that to point at the bottom-left of the html
  // input element rect.
  gfx::NativeView view = tab_contents->view()->GetNativeView();
  NSRect tab_bounds = [view bounds];
  NSPoint anchor = NSMakePoint(
      tab_bounds.origin.x + element_rect.x() + kBubbleTargetOffsetX,
      tab_bounds.origin.y + tab_bounds.size.height - element_rect.y() -
      element_rect.height());
  anchor = [view convertPoint:anchor toView:nil];
  anchor = [[view window] convertBaseToScreen:anchor];

  window_.reset([[SpeechInputWindowController alloc]
      initWithParentWindow:tab_contents->view()->GetTopLevelNativeWindow()
                  delegate:delegate
                anchoredAt:anchor]);
}

SpeechInputBubbleImpl::~SpeechInputBubbleImpl() {
  [window_.get() close];
}

void SpeechInputBubbleImpl::SetImage(const SkBitmap& image) {
  // TODO(satish): Implement.
  NOTREACHED();
}

void SpeechInputBubbleImpl::Show() {
  // TODO(satish): Implement.
  NOTREACHED();
}

void SpeechInputBubbleImpl::Hide() {
  // TODO(satish): Implement.
  NOTREACHED();
}

void SpeechInputBubbleImpl::UpdateLayout() {
  // TODO(satish): Implement.
  NOTREACHED();
}

}  // namespace

SpeechInputBubble* SpeechInputBubble::CreateNativeBubble(
    TabContents* tab_contents,
    Delegate* delegate,
    const gfx::Rect& element_rect) {
  return new SpeechInputBubbleImpl(tab_contents, delegate, element_rect);
}

