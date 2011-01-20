// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/common_param_traits.h"

#define IPC_MESSAGE_IMPL
#include "chrome/common/speech_input_messages.h"

SpeechInputHostMsg_StartRecognition_Params::
SpeechInputHostMsg_StartRecognition_Params()
    : render_view_id(0),
      request_id(0) {
}

SpeechInputHostMsg_StartRecognition_Params::
~SpeechInputHostMsg_StartRecognition_Params() {
}

namespace IPC {

void ParamTraits<speech_input::SpeechInputResultItem>::Write(
    Message* m, const param_type& p) {
  WriteParam(m, p.utterance);
  WriteParam(m, p.confidence);
}

bool ParamTraits<speech_input::SpeechInputResultItem>::Read(const Message* m,
                                                            void** iter,
                                                            param_type* p) {
  return ReadParam(m, iter, &p->utterance) &&
         ReadParam(m, iter, &p->confidence);
}

void ParamTraits<speech_input::SpeechInputResultItem>::Log(const param_type& p,
                                                           std::string* l) {
  l->append("(");
  LogParam(p.utterance, l);
  l->append(":");
  LogParam(p.confidence, l);
  l->append(")");
}

void ParamTraits<SpeechInputHostMsg_StartRecognition_Params>::Write(
    Message* m,
    const param_type& p) {
  WriteParam(m, p.render_view_id);
  WriteParam(m, p.request_id);
  WriteParam(m, p.element_rect);
  WriteParam(m, p.language);
  WriteParam(m, p.grammar);
  WriteParam(m, p.origin_url);
}

bool ParamTraits<SpeechInputHostMsg_StartRecognition_Params>::Read(
    const Message* m, void** iter, param_type* p) {
  return
      ReadParam(m, iter, &p->render_view_id) &&
      ReadParam(m, iter, &p->request_id) &&
      ReadParam(m, iter, &p->element_rect) &&
      ReadParam(m, iter, &p->language) &&
      ReadParam(m, iter, &p->grammar) &&
      ReadParam(m, iter, &p->origin_url);
}

void ParamTraits<SpeechInputHostMsg_StartRecognition_Params>::Log(
    const param_type& p, std::string* l) {
  l->append("(");
  LogParam(p.render_view_id, l);
  l->append(", ");
  LogParam(p.request_id, l);
  l->append(", ");
  LogParam(p.element_rect, l);
  l->append(", ");
  LogParam(p.language, l);
  l->append(", ");
  LogParam(p.grammar, l);
  l->append(", ");
  LogParam(p.origin_url, l);
  l->append(")");
}

}  // namespace IPC
