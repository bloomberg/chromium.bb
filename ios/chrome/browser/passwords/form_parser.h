// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PASSWORDS_FORM_PARSER_H_
#define IOS_CHROME_BROWSER_PASSWORDS_FORM_PARSER_H_

#include <memory>

namespace autofill {
struct FormData;
struct PasswordForm;
}  // namespace autofill

enum class FormParsingMode { FILLING, SAVING };

class FormParser {
 public:
  FormParser();

  // Parse DOM information |form_data| to Password Manager form representation
  // PasswordForm.
  // Return nullptr when parsing is unsuccessful.
  std::unique_ptr<autofill::PasswordForm> Parse(
      const autofill::FormData& form_data,
      FormParsingMode mode);
};

#endif  // IOS_CHROME_BROWSER_PASSWORDS_FORM_PARSER_H_
