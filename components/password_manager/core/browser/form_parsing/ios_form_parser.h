// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_FORM_PARSING_IOS_FORM_PARSER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_FORM_PARSING_IOS_FORM_PARSER_H_

#include <memory>

namespace autofill {
struct FormData;
struct PasswordForm;
}  // namespace autofill

namespace password_manager {

enum class FormParsingMode { FILLING, SAVING };

class IOSFormParser {
 public:
  IOSFormParser();

  // Parse DOM information |form_data| to Password Manager form representation
  // PasswordForm.
  // Return nullptr when parsing is unsuccessful.
  std::unique_ptr<autofill::PasswordForm> Parse(
      const autofill::FormData& form_data,
      FormParsingMode mode);
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_FORM_PARSING_IOS_FORM_PARSER_H_
