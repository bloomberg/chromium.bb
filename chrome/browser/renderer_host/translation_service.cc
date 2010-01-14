// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/translation_service.h"

#include "base/string_util.h"
#include "chrome/browser/renderer_host/resource_message_filter.h"
#include "chrome/common/render_messages.h"

TranslationService::TranslationService(ResourceMessageFilter* filter)
    : resource_message_filter_(filter) {
}

void TranslationService::Translate(int routing_id,
                                   int work_id,
                                   const std::vector<string16>& text_chunks,
                                   std::string from_language,
                                   std::string to_language,
                                   bool secure) {
  std::vector<string16> translated_text;
  for (std::vector<string16>::const_iterator iter = text_chunks.begin();
       iter != text_chunks.end(); ++iter) {
    translated_text.push_back(StringToUpperASCII(*iter));
  }
  resource_message_filter_->Send(
      new ViewMsg_TranslateTextReponse(routing_id, work_id,
                                       0, translated_text));
}
