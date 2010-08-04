// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/speech_input_dispatcher.h"

#include "chrome/renderer/render_view.h"
#include "third_party/WebKit/WebKit/chromium/public/WebCString.h"
#include "third_party/WebKit/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/WebKit/chromium/public/WebSpeechInputListener.h"

using WebKit::WebFrame;

SpeechInputDispatcher::SpeechInputDispatcher(
    RenderView* render_view, WebKit::WebSpeechInputListener* listener)
    : render_view_(render_view),
      listener_(listener) {
}

bool SpeechInputDispatcher::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(SpeechInputDispatcher, message)
    IPC_MESSAGE_HANDLER(ViewMsg_SpeechInput_SetRecognitionResult,
                        OnSpeechRecognitionResult)
    IPC_MESSAGE_HANDLER(ViewMsg_SpeechInput_RecordingComplete,
                        OnSpeechRecordingComplete)
    IPC_MESSAGE_HANDLER(ViewMsg_SpeechInput_RecognitionComplete,
                        OnSpeechRecognitionComplete)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

bool SpeechInputDispatcher::startRecognition() {
  render_view_->Send(new ViewHostMsg_SpeechInput_StartRecognition(
      render_view_->routing_id()));
  return true;
}

void SpeechInputDispatcher::cancelRecognition() {
  render_view_->Send(new ViewHostMsg_SpeechInput_CancelRecognition(
      render_view_->routing_id()));
}

void SpeechInputDispatcher::stopRecording() {
  render_view_->Send(new ViewHostMsg_SpeechInput_StopRecording(
      render_view_->routing_id()));
}

void SpeechInputDispatcher::OnSpeechRecognitionResult(
    const string16& result) {
  listener_->setRecognitionResult(result);
}

void SpeechInputDispatcher::OnSpeechRecordingComplete() {
  listener_->didCompleteRecording();
}

void SpeechInputDispatcher::OnSpeechRecognitionComplete() {
  listener_->didCompleteRecognition();
}
