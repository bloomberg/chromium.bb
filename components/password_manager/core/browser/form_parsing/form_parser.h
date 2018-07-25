// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_FORM_PARSING_FORM_PARSER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_FORM_PARSING_FORM_PARSER_H_

#include <memory>
#include <vector>

#include "components/password_manager/core/browser/form_parsing/password_field_prediction.h"

namespace autofill {
struct FormData;
struct PasswordForm;
}  // namespace autofill

namespace password_manager {

enum class FormParsingMode { FILLING, SAVING };

// This needs to be in sync with the histogram enumeration
// UsernameDetectionMethod, because the values are reported in the
// "PasswordManager.UsernameDetectionMethod" histogram. Don't remove or shift
// existing values in the enum, only append and mark as obsolete as needed.
enum class UsernameDetectionMethod {
  kNoUsernameDetected = 0,
  kBaseHeuristic = 1,
  kHtmlBasedClassifier = 2,
  kAutocompleteAttribute = 3,
  kServerSidePrediction = 4,
  kCount
};

// Parse DOM information |form_data| into Password Manager's form representation
// PasswordForm. |form_predictions| are an optional source of server-side
// predictions about field types. Return nullptr when parsing is unsuccessful.
std::unique_ptr<autofill::PasswordForm> ParseFormData(
    const autofill::FormData& form_data,
    const FormPredictions* form_predictions,
    FormParsingMode mode);

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_FORM_PARSING_IOS_FORM_PARSER_H_
