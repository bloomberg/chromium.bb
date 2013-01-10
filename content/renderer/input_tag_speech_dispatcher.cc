// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/input_tag_speech_dispatcher.h"

#include "base/utf_string_conversions.h"
#include "content/common/speech_recognition_messages.h"
#include "content/renderer/render_view_impl.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebSize.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDocument.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebElement.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebInputElement.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebNode.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSecurityOrigin.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSpeechInputListener.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"

using WebKit::WebDocument;
using WebKit::WebElement;
using WebKit::WebFrame;
using WebKit::WebInputElement;
using WebKit::WebNode;
using WebKit::WebView;

namespace content {

InputTagSpeechDispatcher::InputTagSpeechDispatcher(
    RenderViewImpl* render_view,
    WebKit::WebSpeechInputListener* listener)
    : RenderViewObserver(render_view),
      listener_(listener) {
}

bool InputTagSpeechDispatcher::OnMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(InputTagSpeechDispatcher, message)
    IPC_MESSAGE_HANDLER(InputTagSpeechMsg_SetRecognitionResults,
                        OnSpeechRecognitionResults)
    IPC_MESSAGE_HANDLER(InputTagSpeechMsg_RecordingComplete,
                        OnSpeechRecordingComplete)
    IPC_MESSAGE_HANDLER(InputTagSpeechMsg_RecognitionComplete,
                        OnSpeechRecognitionComplete)
    IPC_MESSAGE_HANDLER(InputTagSpeechMsg_ToggleSpeechInput,
                        OnSpeechRecognitionToggleSpeechInput)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

bool InputTagSpeechDispatcher::startRecognition(
    int request_id,
    const WebKit::WebRect& element_rect,
    const WebKit::WebString& language,
    const WebKit::WebString& grammar,
    const WebKit::WebSecurityOrigin& origin) {
  DVLOG(1) << "InputTagSpeechDispatcher::startRecognition enter";

  InputTagSpeechHostMsg_StartRecognition_Params params;
  params.grammar = UTF16ToUTF8(grammar);
  params.language = UTF16ToUTF8(language);
  params.origin_url = UTF16ToUTF8(origin.toString());
  params.render_view_id = routing_id();
  params.request_id = request_id;
  params.element_rect = element_rect;

  Send(new InputTagSpeechHostMsg_StartRecognition(params));
  DVLOG(1) << "InputTagSpeechDispatcher::startRecognition exit";
  return true;
}

void InputTagSpeechDispatcher::cancelRecognition(int request_id) {
  DVLOG(1) << "InputTagSpeechDispatcher::cancelRecognition enter";
  Send(new InputTagSpeechHostMsg_CancelRecognition(routing_id(), request_id));
  DVLOG(1) << "InputTagSpeechDispatcher::cancelRecognition exit";
}

void InputTagSpeechDispatcher::stopRecording(int request_id) {
  DVLOG(1) << "InputTagSpeechDispatcher::stopRecording enter";
  Send(new InputTagSpeechHostMsg_StopRecording(routing_id(),
                                               request_id));
  DVLOG(1) << "InputTagSpeechDispatcher::stopRecording exit";
}

void InputTagSpeechDispatcher::OnSpeechRecognitionResults(
    int request_id,
    const SpeechRecognitionResults& results) {
  DVLOG(1) << "InputTagSpeechDispatcher::OnSpeechRecognitionResults enter";
  DCHECK_EQ(results.size(), 1U);

  const SpeechRecognitionResult& result = results[0];
  WebKit::WebSpeechInputResultArray webkit_result(result.hypotheses.size());
  for (size_t i = 0; i < result.hypotheses.size(); ++i) {
    webkit_result[i].assign(result.hypotheses[i].utterance,
        result.hypotheses[i].confidence);
  }
  listener_->setRecognitionResult(request_id, webkit_result);

  DVLOG(1) << "InputTagSpeechDispatcher::OnSpeechRecognitionResults exit";
}

void InputTagSpeechDispatcher::OnSpeechRecordingComplete(int request_id) {
  DVLOG(1) << "InputTagSpeechDispatcher::OnSpeechRecordingComplete enter";
  listener_->didCompleteRecording(request_id);
  DVLOG(1) << "InputTagSpeechDispatcher::OnSpeechRecordingComplete exit";
}

void InputTagSpeechDispatcher::OnSpeechRecognitionComplete(int request_id) {
  DVLOG(1) << "InputTagSpeechDispatcher::OnSpeechRecognitionComplete enter";
  listener_->didCompleteRecognition(request_id);
  DVLOG(1) << "InputTagSpeechDispatcher::OnSpeechRecognitionComplete exit";
}

void InputTagSpeechDispatcher::OnSpeechRecognitionToggleSpeechInput() {
  DVLOG(1) <<"InputTagSpeechDispatcher::OnSpeechRecognitionToggleSpeechInput";

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

}  // namespace content
