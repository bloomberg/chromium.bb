// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_TRANSLATE_ERRORS_H_
#define CHROME_COMMON_TRANSLATE_ERRORS_H_

// This file consolidates all the error types for translation of a page.

class TranslateErrors {
 public:
  enum Type {
    NONE = 0,
    NETWORK = 1,
    SERVER,
  };

 private:
  TranslateErrors() {}

  DISALLOW_COPY_AND_ASSIGN(TranslateErrors);
};

#endif  // CHROME_COMMON_TRANSLATE_ERRORS_H_
