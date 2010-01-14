// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDERER_HOST_TRANSLATION_SERVICE_H_
#define CHROME_BROWSER_RENDERER_HOST_TRANSLATION_SERVICE_H_

#include <string>
#include <vector>

#include "base/string16.h"

class ResourceMessageFilter;

// The TranslationService class is used to translate text.
// This temporary implementation only upcases the text sent to it.
class TranslationService {
 public:
  explicit TranslationService(ResourceMessageFilter* resource_msg_filter);

  // Translates the passed text chunks and sends a
  // ViewMsg_TranslateTextReponse message on the renderer at |routing_id|.
  void Translate(int routing_id,
                 int work_id,
                 const std::vector<string16>& text_chunks,
                 std::string from_language,
                 std::string to_language,
                 bool secure);

 private:
  ResourceMessageFilter* resource_message_filter_;

  DISALLOW_COPY_AND_ASSIGN(TranslationService);
};

#endif  // CHROME_BROWSER_RENDERER_HOST_TRANSLATION_SERVICE_H_
