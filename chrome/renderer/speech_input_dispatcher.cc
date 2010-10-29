// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/speech_input_dispatcher.h"

#include "base/utf_string_conversions.h"
#include "chrome/renderer/render_view.h"
#include "third_party/WebKit/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/WebKit/chromium/public/WebSpeechInputListener.h"
#include "third_party/WebKit/WebKit/chromium/public/WebSize.h"
#include "third_party/WebKit/WebKit/chromium/public/WebString.h"
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
    int request_id,
    const WebKit::WebRect& element_rect,
    const WebKit::WebString& language,
    const WebKit::WebString& grammar) {
  return startRecognition(request_id, element_rect, grammar);
}

bool SpeechInputDispatcher::startRecognition(
    int request_id,
    const WebKit::WebRect& element_rect,
    const WebKit::WebString& grammar) {
  VLOG(1) << "SpeechInputDispatcher::startRecognition enter";
  gfx::Size scroll = render_view_->webview()->mainFrame()->scrollOffset();
  gfx::Rect rect = element_rect;
  rect.Offset(-scroll.width(), -scroll.height());
  render_view_->Send(new ViewHostMsg_SpeechInput_StartRecognition(
      render_view_->routing_id(), request_id, rect, UTF16ToUTF8(grammar)));
  VLOG(1) << "SpeechInputDispatcher::startRecognition exit";
  return true;
}

void SpeechInputDispatcher::cancelRecognition(int request_id) {
  VLOG(1) << "SpeechInputDispatcher::cancelRecognition enter";
  render_view_->Send(new ViewHostMsg_SpeechInput_CancelRecognition(
      render_view_->routing_id(), request_id));
  VLOG(1) << "SpeechInputDispatcher::cancelRecognition exit";
}

void SpeechInputDispatcher::stopRecording(int request_id) {
  VLOG(1) << "SpeechInputDispatcher::stopRecording enter";
  render_view_->Send(new ViewHostMsg_SpeechInput_StopRecording(
      render_view_->routing_id(), request_id));
  VLOG(1) << "SpeechInputDispatcher::stopRecording exit";
}

void SpeechInputDispatcher::OnSpeechRecognitionResult(
    int request_id, const speech_input::SpeechInputResultArray& result) {
  VLOG(1) << "SpeechInputDispatcher::OnSpeechRecognitionResult enter";
  WebKit::WebSpeechInputResultArray webkit_result(result.size());
  for (size_t i = 0; i < result.size(); ++i)
    webkit_result[i].set(result[i].utterance, result[i].confidence);
  listener_->setRecognitionResult(request_id, webkit_result);
  VLOG(1) << "SpeechInputDispatcher::OnSpeechRecognitionResult exit";
}

void SpeechInputDispatcher::OnSpeechRecordingComplete(int request_id) {
  VLOG(1) << "SpeechInputDispatcher::OnSpeechRecordingComplete enter";
  listener_->didCompleteRecording(request_id);
  VLOG(1) << "SpeechInputDispatcher::OnSpeechRecordingComplete exit";
}

void SpeechInputDispatcher::OnSpeechRecognitionComplete(int request_id) {
  VLOG(1) << "SpeechInputDispatcher::OnSpeechRecognitionComplete enter";
  listener_->didCompleteRecognition(request_id);
  VLOG(1) << "SpeechInputDispatcher::OnSpeechRecognitionComplete exit";
}
