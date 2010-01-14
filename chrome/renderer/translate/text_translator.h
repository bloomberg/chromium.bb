// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_TRANSLATE_TEXT_TRANSLATOR_H_
#define CHROME_RENDERER_TRANSLATE_TEXT_TRANSLATOR_H_

#include <string>
#include <vector>

#include "base/string16.h"

// TextTranslator is an interface that is implemented by providers that know
// how to translate text from one language to another.
// It is asynchronous. Clients call Translate() with the text to translate and
// receive a work id. The implementation should call the TextTranslated
// method on the delegate once the text has been translated.

class TextTranslator {
 public:
  class Delegate {
   public:
    virtual ~Delegate() {}

    // Notifies that the translation failed for |work_id|.
    virtual void TranslationError(int work_id, int error_id) = 0;

    // Notifies that the translation for |work_id| succeeded.
    virtual void TextTranslated(
        int work_id, const std::vector<string16>& translated_text) = 0;
  };

  TextTranslator() {}
  virtual ~TextTranslator() {}

  // Initiates the translation of the |text| provided, from the language
  // |from_lang| to |to_lang| (these are the ISO language code, for example en,
  // fr, ja...).  If |secure| is true then a secure communication method (HTTPS)
  // should be used if using a remote resource to perform the translation.
  // Returns a work id that is passed as a parameter when delegate methods are
  // called on |delegate| to notify the translation succeeded/failed.
  virtual int Translate(const std::vector<string16>& text,
                        std::string from_lang,
                        std::string to_lang,
                        bool secure,
                        TextTranslator::Delegate* delegate) = 0;
};

#endif  // CHROME_RENDERER_TRANSLATE_TEXT_TRANSLATOR_H_
