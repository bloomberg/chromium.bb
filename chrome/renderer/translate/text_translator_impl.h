// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_TRANSLATE_TEXT_TRANSLATOR_IMPL_H_
#define CHROME_RENDERER_TRANSLATE_TEXT_TRANSLATOR_IMPL_H_

#include <string>
#include <vector>

#include "base/logging.h"
#include "chrome/renderer/translate/text_translator.h"

class RenderView;

// An implementation of the TextTranslator that sends translation requests
// to the browser.
// There is one instance of TextTranslatorImpl per RenderViewHost and the
// RenderViewHost owns that instance.
//
// TODO(jcampan): limit the number of translation requests in flight so not to
//                swamp the browser's resource dispatcher host.

class TextTranslatorImpl : public TextTranslator {
 public:
  explicit TextTranslatorImpl(RenderView* render_view);

  // Called by the renderer to notify a translation response has been received.
  // |error_id| is different than 0 if an error occurred.
  void OnTranslationResponse(int work_id,
                             int error_id,
                             const std::vector<string16>& text_chunks);

  // TextTranslator implementation.
  virtual int Translate(const std::vector<string16>& text,
                        std::string from_lang,
                        std::string to_lang,
                        bool secure,
                        TextTranslator::Delegate* delegate);
 private:
  // The render view through which translation requests/responses are
  // sent/received.
  RenderView* render_view_;

  // The counter used to create work ids.
  int work_id_counter_;

  DISALLOW_COPY_AND_ASSIGN(TextTranslatorImpl);
};

#endif  // CHROME_RENDERER_TRANSLATE_TEXT_TRANSLATOR_IMPL_H_
