// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/speech/speech_input_bubble_controller.h"

#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "gfx/rect.h"

namespace speech_input {

SpeechInputBubbleController::SpeechInputBubbleController(Delegate* delegate)
    : delegate_(delegate) {
}

void SpeechInputBubbleController::CreateBubble(int caller_id,
                                               int render_process_id,
                                               int render_view_id,
                                               const gfx::Rect& element_rect) {
  if (!ChromeThread::CurrentlyOn(ChromeThread::UI)) {
    ChromeThread::PostTask(
        ChromeThread::UI, FROM_HERE,
        NewRunnableMethod(this, &SpeechInputBubbleController::CreateBubble,
                          caller_id, render_process_id, render_view_id,
                          element_rect));
    return;
  }
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  TabContents* tab_contents = tab_util::GetTabContentsByID(render_process_id,
                                                           render_view_id);

  DCHECK(!bubble_.get());
  bubble_.reset(SpeechInputBubble::Create(tab_contents, this, element_rect));
  if (!bubble_.get())  // could be null if tab or display rect were invalid.
    return;

  current_bubble_caller_id_ = caller_id;
}

void SpeechInputBubbleController::CloseBubble(int caller_id) {
  if (!ChromeThread::CurrentlyOn(ChromeThread::UI)) {
    ChromeThread::PostTask(
        ChromeThread::UI, FROM_HERE,
        NewRunnableMethod(this, &SpeechInputBubbleController::CloseBubble,
                          caller_id));
    return;
  }
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  if (current_bubble_caller_id_ != caller_id)
    return;

  current_bubble_caller_id_ = 0;

  DCHECK(bubble_.get());
  bubble_.reset();
}

void SpeechInputBubbleController::SetBubbleToRecognizingMode(int caller_id) {
  if (!ChromeThread::CurrentlyOn(ChromeThread::UI)) {
    ChromeThread::PostTask(ChromeThread::UI, FROM_HERE, NewRunnableMethod(
        this, &SpeechInputBubbleController::SetBubbleToRecognizingMode,
        caller_id));
    return;
  }
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  if (current_bubble_caller_id_ != caller_id)
    return;

  DCHECK(bubble_.get());
  bubble_->SetRecognizingMode();
}

void SpeechInputBubbleController::RecognitionCancelled() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));

  int old_bubble_caller_id = current_bubble_caller_id_;
  current_bubble_caller_id_ = 0;
  bubble_.reset();

  ChromeThread::PostTask(
      ChromeThread::IO, FROM_HERE,
      NewRunnableMethod(
          this,
          &SpeechInputBubbleController::InvokeDelegateRecognitionCancelled,
          old_bubble_caller_id));
}

void SpeechInputBubbleController::InfoBubbleClosed() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));

  int old_bubble_caller_id = current_bubble_caller_id_;
  current_bubble_caller_id_ = 0;
  bubble_.reset();

  ChromeThread::PostTask(
      ChromeThread::IO, FROM_HERE,
      NewRunnableMethod(
          this,
          &SpeechInputBubbleController::InvokeDelegateFocusChanged,
          old_bubble_caller_id));
}

void SpeechInputBubbleController::InvokeDelegateRecognitionCancelled(
    int caller_id) {
  delegate_->RecognitionCancelled(caller_id);
}

void SpeechInputBubbleController::InvokeDelegateFocusChanged(int caller_id) {
  delegate_->SpeechInputFocusChanged(caller_id);
}

}  // namespace speech_input
