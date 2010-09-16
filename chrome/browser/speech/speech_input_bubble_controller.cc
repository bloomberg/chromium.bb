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
  ProcessRequestInUiThread(caller_id, REQUEST_CLOSE, string16(), 0);
}

void SpeechInputBubbleController::SetBubbleRecordingMode(int caller_id) {
  ProcessRequestInUiThread(caller_id, REQUEST_SET_RECORDING_MODE,
                           string16(), 0);
}

void SpeechInputBubbleController::SetBubbleRecognizingMode(int caller_id) {
  ProcessRequestInUiThread(caller_id, REQUEST_SET_RECOGNIZING_MODE,
                           string16(), 0);
}

void SpeechInputBubbleController::SetBubbleInputVolume(int caller_id,
                                                       float volume) {
  ProcessRequestInUiThread(caller_id, REQUEST_SET_INPUT_VOLUME, string16(),
                           volume);
}

void SpeechInputBubbleController::SetBubbleMessage(int caller_id,
                                                   const string16& text) {
  ProcessRequestInUiThread(caller_id, REQUEST_SET_MESSAGE, text, 0);
}

void SpeechInputBubbleController::ProcessRequestInUiThread(
    int caller_id, RequestType type, const string16& text, float volume) {
  if (!ChromeThread::CurrentlyOn(ChromeThread::UI)) {
    ChromeThread::PostTask(ChromeThread::UI, FROM_HERE, NewRunnableMethod(
        this, &SpeechInputBubbleController::ProcessRequestInUiThread,
        caller_id, type, text, volume));
    return;
  }
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  // The bubble may have been closed before we got a chance to process this
  // request. So check before proceeding.
  if (!bubbles_.count(caller_id))
    return;

  bool change_active_bubble = (type == REQUEST_SET_RECORDING_MODE ||
                               type == REQUEST_SET_MESSAGE);
  if (change_active_bubble) {
    if (current_bubble_caller_id_ && current_bubble_caller_id_ != caller_id)
      bubbles_[current_bubble_caller_id_]->Hide();
    current_bubble_caller_id_ = caller_id;
  }

  SpeechInputBubble* bubble = bubbles_[caller_id];
  switch (type) {
    case REQUEST_SET_RECORDING_MODE:
      bubble->SetRecordingMode();
      break;
    case REQUEST_SET_RECOGNIZING_MODE:
      bubble->SetRecognizingMode();
      break;
    case REQUEST_SET_MESSAGE:
      bubble->SetMessage(text);
      break;
    case REQUEST_SET_INPUT_VOLUME:
      bubble->SetInputVolume(volume);
      break;
    case REQUEST_CLOSE:
      if (current_bubble_caller_id_ == caller_id)
        current_bubble_caller_id_ = 0;
      delete bubble;
      bubbles_.erase(caller_id);
      break;
    default:
      NOTREACHED();
      break;
  }

  if (change_active_bubble)
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
