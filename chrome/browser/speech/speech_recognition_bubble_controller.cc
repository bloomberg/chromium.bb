// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/speech/speech_recognition_bubble_controller.h"

#include "base/bind.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"

using content::BrowserThread;
using content::WebContents;

namespace {
const int kInvalidSessionId = 0;
}

namespace speech {

SpeechRecognitionBubbleController::SpeechRecognitionBubbleController(
    Delegate* delegate)
    : delegate_(delegate),
      current_bubble_session_id_(kInvalidSessionId),
      current_bubble_render_process_id_(0),
      current_bubble_render_view_id_(0),
      last_request_issued_(REQUEST_CLOSE) {
}

SpeechRecognitionBubbleController::~SpeechRecognitionBubbleController() {
  DCHECK_EQ(kInvalidSessionId, current_bubble_session_id_);
}

void SpeechRecognitionBubbleController::CreateBubble(
    int session_id,
    int render_process_id,
    int render_view_id,
    const gfx::Rect& element_rect) {
  current_bubble_session_id_ = session_id;
  current_bubble_render_process_id_ = render_process_id;
  current_bubble_render_view_id_ = render_view_id;

  UIRequest request(REQUEST_CREATE);
  request.render_process_id = render_process_id;
  request.render_view_id = render_view_id;
  request.element_rect = element_rect;
  ProcessRequestInUiThread(request);
}

void SpeechRecognitionBubbleController::SetBubbleRecordingMode() {
  ProcessRequestInUiThread(UIRequest(REQUEST_SET_RECORDING_MODE));
}

void SpeechRecognitionBubbleController::SetBubbleRecognizingMode() {
  ProcessRequestInUiThread(UIRequest(REQUEST_SET_RECOGNIZING_MODE));
}

void SpeechRecognitionBubbleController::SetBubbleMessage(const string16& text) {
  UIRequest request(REQUEST_SET_MESSAGE);
  request.message = text;
  ProcessRequestInUiThread(request);
}

bool SpeechRecognitionBubbleController::IsShowingMessage() const {
  return last_request_issued_ == REQUEST_SET_MESSAGE;
}

void SpeechRecognitionBubbleController::SetBubbleInputVolume(
    float volume,
    float noise_volume) {
  UIRequest request(REQUEST_SET_INPUT_VOLUME);
  request.volume = volume;
  request.noise_volume = noise_volume;
  ProcessRequestInUiThread(request);
}

void SpeechRecognitionBubbleController::CloseBubble() {
  current_bubble_session_id_ = kInvalidSessionId;
  ProcessRequestInUiThread(UIRequest(REQUEST_CLOSE));
}

int SpeechRecognitionBubbleController::GetActiveSessionID() const {
  return current_bubble_session_id_;
}

bool SpeechRecognitionBubbleController::IsShowingBubbleForRenderView(
    int render_process_id,
    int render_view_id) {
  return (current_bubble_session_id_ != kInvalidSessionId) &&
      (current_bubble_render_process_id_ == render_process_id) &&
      (current_bubble_render_view_id_ == render_view_id);
}

void SpeechRecognitionBubbleController::InfoBubbleButtonClicked(
    SpeechRecognitionBubble::Button button) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
      base::Bind(
          &SpeechRecognitionBubbleController::InvokeDelegateButtonClicked, this,
          button));
}

void SpeechRecognitionBubbleController::InfoBubbleFocusChanged() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
      base::Bind(&SpeechRecognitionBubbleController::InvokeDelegateFocusChanged,
                 this));
}

void SpeechRecognitionBubbleController::InvokeDelegateButtonClicked(
    SpeechRecognitionBubble::Button button) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK_NE(kInvalidSessionId, current_bubble_session_id_);
  delegate_->InfoBubbleButtonClicked(current_bubble_session_id_, button);
}

void SpeechRecognitionBubbleController::InvokeDelegateFocusChanged() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK_NE(kInvalidSessionId, current_bubble_session_id_);
  delegate_->InfoBubbleFocusChanged(current_bubble_session_id_);
}

void SpeechRecognitionBubbleController::ProcessRequestInUiThread(
    const UIRequest& request) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    last_request_issued_ = request.type;
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
        base::Bind(&SpeechRecognitionBubbleController::ProcessRequestInUiThread,
                   this, request));
    return;
  }

  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  switch (request.type) {
    case REQUEST_CREATE:
      bubble_.reset(SpeechRecognitionBubble::Create(
          tab_util::GetWebContentsByID(request.render_process_id,
                                       request.render_view_id),
          this, request.element_rect));

      if (!bubble_.get()) {
        // Could be null if tab or display rect were invalid.
        // Simulate the cancel button being clicked to inform the delegate.
        BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
            base::Bind(
                &SpeechRecognitionBubbleController::InvokeDelegateButtonClicked,
                this, SpeechRecognitionBubble::BUTTON_CANCEL));
        return;
      }
      bubble_->Show();
      bubble_->SetWarmUpMode();
      break;
    case REQUEST_SET_RECORDING_MODE:
      if (!bubble_.get()) {
        // We've seen this on crash/ but don't yet understand why this happen.
        // TODO(tommi): It would be nice to figure this out properly and fix
        // but since this feature is being deprecated, removing the code
        // altogether is probably a simpler and more permanent fix :)
        DLOG(ERROR) << "NULL bubble in REQUEST_SET_RECORDING_MODE!";
      } else {
        bubble_->SetRecordingMode();
      }
      break;
    case REQUEST_SET_RECOGNIZING_MODE:
      DCHECK(bubble_.get());
      bubble_->SetRecognizingMode();
      break;
    case REQUEST_SET_MESSAGE:
      DCHECK(bubble_.get());
      bubble_->SetMessage(request.message);
      break;
    case REQUEST_SET_INPUT_VOLUME:
      DCHECK(bubble_.get());
      bubble_->SetInputVolume(request.volume, request.noise_volume);
      break;
    case REQUEST_CLOSE:
      bubble_.reset();
      break;
    default:
      NOTREACHED();
      break;
  }
}

SpeechRecognitionBubbleController::UIRequest::UIRequest(RequestType type_value)
    : type(type_value),
      volume(0.0F),
      noise_volume(0.0F),
      render_process_id(0),
      render_view_id(0) {
}

SpeechRecognitionBubbleController::UIRequest::~UIRequest() {
}

}  // namespace speech
