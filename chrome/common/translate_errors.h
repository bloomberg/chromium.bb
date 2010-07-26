// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_TRANSLATE_ERRORS_H_
#define CHROME_COMMON_TRANSLATE_ERRORS_H_
#pragma once

// This file consolidates all the error types for translation of a page.

class TranslateErrors {
 public:
  enum Type {
    NONE = 0,
    NETWORK,  // No connectivity.
    INITIALIZATION_ERROR,  // The translation script failed to initialize.
    UNKNOWN_LANGUAGE,      // The page's language could not be detected.
    UNSUPPORTED_LANGUAGE,  // The server detected a language that the browser
                           // does not know.
    IDENTICAL_LANGUAGES,   // The original and target languages are the same.
    TRANSLATION_ERROR,     // An error was reported by the translation script
                           // during translation.
  };

 private:
  TranslateErrors() {}

  DISALLOW_COPY_AND_ASSIGN(TranslateErrors);
};

#endif  // CHROME_COMMON_TRANSLATE_ERRORS_H_
