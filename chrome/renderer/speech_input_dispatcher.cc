// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/speech_input_dispatcher.h"

#include "chrome/renderer/render_view.h"
#include "third_party/WebKit/WebKit/chromium/public/WebCString.h"
#include "third_party/WebKit/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/WebKit/chromium/public/WebSpeechInputListener.h"
#include "third_party/WebKit/WebKit/chromium/public/WebSize.h"
#include "third_party/WebKit/WebKit/chromium/public/WebView.h"

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

bool SpeechInputDispatcher::startRecognition(
    int request_id, const WebKit::WebRect& element_rect) {
  LOG(INFO) << "SpeechInputDispatcher::startRecognition enter";
  gfx::Size scroll = render_view_->webview()->mainFrame()->scrollOffset();
  gfx::Rect rect = element_rect;
  rect.Offset(-scroll.width(), -scroll.height());
  render_view_->Send(new ViewHostMsg_SpeechInput_StartRecognition(
      render_view_->routing_id(), request_id, rect));
  LOG(INFO) << "SpeechInputDispatcher::startRecognition exit";
  return true;
}

void SpeechInputDispatcher::cancelRecognition(int request_id) {
  LOG(INFO) << "SpeechInputDispatcher::cancelRecognition enter";
  render_view_->Send(new ViewHostMsg_SpeechInput_CancelRecognition(
      render_view_->routing_id(), request_id));
  LOG(INFO) << "SpeechInputDispatcher::cancelRecognition exit";
}

void SpeechInputDispatcher::stopRecording(int request_id) {
  LOG(INFO) << "SpeechInputDispatcher::stopRecording enter";
  render_view_->Send(new ViewHostMsg_SpeechInput_StopRecording(
      render_view_->routing_id(), request_id));
  LOG(INFO) << "SpeechInputDispatcher::stopRecording exit";
}

void SpeechInputDispatcher::OnSpeechRecognitionResult(
    int request_id, const string16& result) {
  LOG(INFO) << "SpeechInputDispatcher::OnSpeechRecognitionResult enter";
  listener_->setRecognitionResult(request_id, result);
  LOG(INFO) << "SpeechInputDispatcher::OnSpeechRecognitionResult exit";
}

void SpeechInputDispatcher::OnSpeechRecordingComplete(int request_id) {
  LOG(INFO) << "SpeechInputDispatcher::OnSpeechRecordingComplete enter";
  listener_->didCompleteRecording(request_id);
  LOG(INFO) << "SpeechInputDispatcher::OnSpeechRecordingComplete exit";
}

void SpeechInputDispatcher::OnSpeechRecognitionComplete(int request_id) {
  LOG(INFO) << "SpeechInputDispatcher::OnSpeechRecognitionComplete enter";
  listener_->didCompleteRecognition(request_id);
  LOG(INFO) << "SpeechInputDispatcher::OnSpeechRecognitionComplete exit";
}
