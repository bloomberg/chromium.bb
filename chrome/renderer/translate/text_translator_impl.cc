// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/translate/text_translator_impl.h"

#include "chrome/common/render_messages.h"
#include "chrome/renderer/render_view.h"

TextTranslatorImpl::TextTranslatorImpl(RenderView* render_view)
    : render_view_(render_view),
      work_id_counter_(0) {
}

void TextTranslatorImpl::OnTranslationResponse(
    int work_id, int error_id, const std::vector<string16>& text_chunks) {
  if (error_id) {
    render_view_->page_translator()->TranslationError(work_id, error_id);
    return;
  }
  render_view_->page_translator()->TextTranslated(work_id, text_chunks);
}

int TextTranslatorImpl::Translate(const std::vector<string16>& text,
                                  std::string from_lang,
                                  std::string to_lang,
                                  bool secure,
                                  TextTranslator::Delegate* delegate) {
  ViewHostMsg_TranslateTextParam param;
  param.routing_id = render_view_->routing_id();
  param.work_id = work_id_counter_++;
  param.from_language = from_lang;
  param.to_language = to_lang;
  param.text_chunks = text;
  param.secure = secure;

  render_view_->Send(new ViewHostMsg_TranslateText(param));

  return param.work_id;
}
