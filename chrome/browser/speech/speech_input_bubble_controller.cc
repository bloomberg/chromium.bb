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
    : delegate_(delegate),
      current_bubble_caller_id_(0) {
}

SpeechInputBubbleController::~SpeechInputBubbleController() {
  DCHECK(bubbles_.size() == 0);
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

  DCHECK_EQ(0u, bubbles_.count(caller_id));
  SpeechInputBubble* bubble = SpeechInputBubble::Create(tab_contents, this,
                                                        element_rect);
  if (!bubble)  // could be null if tab or display rect were invalid.
    return;

  bubbles_[caller_id] = bubble;
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

  if (current_bubble_caller_id_ == caller_id)
    current_bubble_caller_id_ = 0;
  delete bubbles_[caller_id];
  bubbles_.erase(caller_id);
}

void SpeechInputBubbleController::SetBubbleRecordingMode(int caller_id) {
  if (!ChromeThread::CurrentlyOn(ChromeThread::UI)) {
    ChromeThread::PostTask(ChromeThread::UI, FROM_HERE, NewRunnableMethod(
        this, &SpeechInputBubbleController::SetBubbleRecordingMode,
        caller_id));
    return;
  }
  SetBubbleRecordingModeOrMessage(caller_id, string16());
}

void SpeechInputBubbleController::SetBubbleRecognizingMode(int caller_id) {
  if (!ChromeThread::CurrentlyOn(ChromeThread::UI)) {
    ChromeThread::PostTask(ChromeThread::UI, FROM_HERE, NewRunnableMethod(
        this, &SpeechInputBubbleController::SetBubbleRecognizingMode,
        caller_id));
    return;
  }
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  // The bubble may have been closed before we got a chance to process this
  // request. So check before proceeding.
  if (!bubbles_.count(caller_id))
    return;

  bubbles_[caller_id]->SetRecognizingMode();
}

void SpeechInputBubbleController::SetBubbleMessage(int caller_id,
                                                   const string16& text) {
  if (!ChromeThread::CurrentlyOn(ChromeThread::UI)) {
    ChromeThread::PostTask(ChromeThread::UI, FROM_HERE, NewRunnableMethod(
        this, &SpeechInputBubbleController::SetBubbleMessage,
        caller_id, text));
    return;
  }
  SetBubbleRecordingModeOrMessage(caller_id, text);
}

void SpeechInputBubbleController::SetBubbleRecordingModeOrMessage(
    int caller_id, const string16& text) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  // The bubble may have been closed before we got a chance to process this
  // request. So check before proceeding.
  if (!bubbles_.count(caller_id))
    return;

  if (current_bubble_caller_id_ && current_bubble_caller_id_ != caller_id)
    bubbles_[current_bubble_caller_id_]->Hide();

  current_bubble_caller_id_ = caller_id;
  SpeechInputBubble* bubble = bubbles_[caller_id];
  if (text.empty()) {
    bubble->SetRecordingMode();
  } else {
    bubble->SetMessage(text);
  }
  bubble->Show();
}

void SpeechInputBubbleController::InfoBubbleButtonClicked(
    SpeechInputBubble::Button button) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  DCHECK(current_bubble_caller_id_);

  ChromeThread::PostTask(
      ChromeThread::IO, FROM_HERE,
      NewRunnableMethod(
          this,
          &SpeechInputBubbleController::InvokeDelegateButtonClicked,
          current_bubble_caller_id_, button));
}

void SpeechInputBubbleController::InfoBubbleFocusChanged() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  DCHECK(current_bubble_caller_id_);

  int old_bubble_caller_id = current_bubble_caller_id_;
  current_bubble_caller_id_ = 0;
  bubbles_[old_bubble_caller_id]->Hide();

  ChromeThread::PostTask(
      ChromeThread::IO, FROM_HERE,
      NewRunnableMethod(
          this,
          &SpeechInputBubbleController::InvokeDelegateFocusChanged,
          old_bubble_caller_id));
}

void SpeechInputBubbleController::InvokeDelegateButtonClicked(
    int caller_id, SpeechInputBubble::Button button) {
  delegate_->InfoBubbleButtonClicked(caller_id, button);
}

void SpeechInputBubbleController::InvokeDelegateFocusChanged(int caller_id) {
  delegate_->InfoBubbleFocusChanged(caller_id);
}

}  // namespace speech_input
