// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/speech/speech_input_bubble.h"
#include "gfx/rect.h"

SpeechInputBubble::FactoryMethod SpeechInputBubble::factory_ = NULL;
const int SpeechInputBubble::kBubbleTargetOffsetX = 5;

SpeechInputBubble* SpeechInputBubble::Create(TabContents* tab_contents,
                                             Delegate* delegate,
                                             const gfx::Rect& element_rect) {
  if (factory_)
    return (*factory_)(tab_contents, delegate, element_rect);

  // Has the tab already closed before bubble create request was processed?
  if (!tab_contents)
    return NULL;

  return CreateNativeBubble(tab_contents, delegate, element_rect);
}

SpeechInputBubbleBase::SpeechInputBubbleBase()
    : display_mode_(DISPLAY_MODE_RECORDING) {
}

void SpeechInputBubbleBase::SetRecordingMode() {
  display_mode_ = DISPLAY_MODE_RECORDING;
  UpdateLayout();
}

void SpeechInputBubbleBase::SetRecognizingMode() {
  display_mode_ = DISPLAY_MODE_RECOGNIZING;
  UpdateLayout();
}

void SpeechInputBubbleBase::SetMessage(const string16& text) {
  message_text_ = text;
  display_mode_ = DISPLAY_MODE_MESSAGE;
  UpdateLayout();
}
