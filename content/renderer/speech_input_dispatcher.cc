// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/speech_input_dispatcher.h"

#include "base/utf_string_conversions.h"
#include "content/common/speech_input_messages.h"
#include "content/renderer/render_view_impl.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDocument.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebElement.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebInputElement.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebNode.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebSize.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSecurityOrigin.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSpeechInputListener.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"

using WebKit::WebDocument;
using WebKit::WebElement;
using WebKit::WebFrame;
using WebKit::WebInputElement;
using WebKit::WebNode;
using WebKit::WebView;

SpeechInputDispatcher::SpeechInputDispatcher(
    RenderViewImpl* render_view,
    WebKit::WebSpeechInputListener* listener)
    : content::RenderViewObserver(render_view),
      listener_(listener) {
}

bool SpeechInputDispatcher::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(SpeechInputDispatcher, message)
    IPC_MESSAGE_HANDLER(SpeechInputMsg_SetRecognitionResult,
                        OnSpeechRecognitionResult)
    IPC_MESSAGE_HANDLER(SpeechInputMsg_RecordingComplete,
                        OnSpeechRecordingComplete)
    IPC_MESSAGE_HANDLER(SpeechInputMsg_RecognitionComplete,
                        OnSpeechRecognitionComplete)
    IPC_MESSAGE_HANDLER(SpeechInputMsg_ToggleSpeechInput,
                        OnSpeechRecognitionToggleSpeechInput)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

bool SpeechInputDispatcher::startRecognition(
    int request_id,
    const WebKit::WebRect& element_rect,
    const WebKit::WebString& language,
    const WebKit::WebString& grammar,
    const WebKit::WebSecurityOrigin& origin) {
  VLOG(1) << "SpeechInputDispatcher::startRecognition enter";

  SpeechInputHostMsg_StartRecognition_Params params;
  params.grammar = UTF16ToUTF8(grammar);
  params.language = UTF16ToUTF8(language);
  params.origin_url = UTF16ToUTF8(origin.toString());
  params.render_view_id = routing_id();
  params.request_id = request_id;
  gfx::Size scroll = render_view()->GetWebView()->mainFrame()->scrollOffset();
  params.element_rect = element_rect;
  params.element_rect.Offset(-scroll.width(), -scroll.height());

  Send(new SpeechInputHostMsg_StartRecognition(params));
  VLOG(1) << "SpeechInputDispatcher::startRecognition exit";
  return true;
}

void SpeechInputDispatcher::cancelRecognition(int request_id) {
  VLOG(1) << "SpeechInputDispatcher::cancelRecognition enter";
  Send(new SpeechInputHostMsg_CancelRecognition(routing_id(), request_id));
  VLOG(1) << "SpeechInputDispatcher::cancelRecognition exit";
}

void SpeechInputDispatcher::stopRecording(int request_id) {
  VLOG(1) << "SpeechInputDispatcher::stopRecording enter";
  Send(new SpeechInputHostMsg_StopRecording(routing_id(), request_id));
  VLOG(1) << "SpeechInputDispatcher::stopRecording exit";
}

void SpeechInputDispatcher::OnSpeechRecognitionResult(
    int request_id,
    const content::SpeechInputResult& result) {
  VLOG(1) << "SpeechInputDispatcher::OnSpeechRecognitionResult enter";
  WebKit::WebSpeechInputResultArray webkit_result(result.hypotheses.size());
  for (size_t i = 0; i < result.hypotheses.size(); ++i) {
    webkit_result[i].set(result.hypotheses[i].utterance,
        result.hypotheses[i].confidence);
  }
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

void SpeechInputDispatcher::OnSpeechRecognitionToggleSpeechInput() {
  VLOG(1) << "SpeechInputDispatcher::OnSpeechRecognitionToggleSpeechInput";

  WebView* web_view = render_view()->GetWebView();

  WebFrame* frame = web_view->mainFrame();
  if (!frame)
    return;

  WebDocument document = frame->document();
  if (document.isNull())
    return;

  WebNode focused_node = document.focusedNode();
  if (focused_node.isNull() || !focused_node.isElementNode())
    return;

  WebKit::WebElement element = focused_node.to<WebKit::WebElement>();
  WebKit::WebInputElement* input_element = WebKit::toWebInputElement(&element);
  if (!input_element)
    return;
  if (!input_element->isSpeechInputEnabled())
    return;

  if (input_element->getSpeechInputState() == WebInputElement::Idle) {
    input_element->startSpeechInput();
  } else {
    input_element->stopSpeechInput();
  }
}
