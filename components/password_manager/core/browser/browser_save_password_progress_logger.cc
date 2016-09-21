// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/browser_save_password_progress_logger.h"

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/log_manager.h"
#include "components/password_manager/core/browser/password_manager.h"

namespace password_manager {

namespace {

// Replaces all non-digits in |str| by spaces.
std::string ScrubNonDigit(std::string str) {
  std::replace_if(str.begin(), str.end(),
                  [](char c) { return !base::IsAsciiDigit(c); }, ' ');
  return str;
}

}  // namespace

BrowserSavePasswordProgressLogger::BrowserSavePasswordProgressLogger(
    const LogManager* log_manager)
    : log_manager_(log_manager) {
  DCHECK(log_manager_);
}

void BrowserSavePasswordProgressLogger::LogFormSignatures(
    SavePasswordProgressLogger::StringID label,
    const autofill::PasswordForm& form) {
  autofill::FormStructure form_structure(form.form_data);
  std::string message = GetStringFromID(label) + ": {\n";
  message += GetStringFromID(STRING_FORM_SIGNATURE) + ": " +
             ScrubNonDigit(form_structure.FormSignatureAsStr()) + "\n";
  message += GetStringFromID(STRING_SIGNON_REALM) + ": " +
             ScrubURL(GURL(form.signon_realm)) + "\n";
  message +=
      GetStringFromID(STRING_ORIGIN) + ": " + ScrubURL(form.origin) + "\n";
  message +=
      GetStringFromID(STRING_ACTION) + ": " + ScrubURL(form.action) + "\n";
  message += GetStringFromID(STRING_FORM_NAME) + ": " +
             ScrubElementID(form.form_data.name) + "\n";
  message += GetStringFromID(STRING_FIELDS) + ": " + "\n";

  for (const autofill::AutofillField* field : form_structure) {
    std::string field_info = ScrubElementID(field->name) + ": " +
                             ScrubNonDigit(field->FieldSignatureAsStr()) +
                             ", " + ScrubElementID(field->form_control_type);
    if (!field->autocomplete_attribute.empty())
      field_info += ", " + ScrubElementID(field->autocomplete_attribute);
    message += field_info + "\n";
  }
  message += "}";
  SendLog(message);
}

BrowserSavePasswordProgressLogger::~BrowserSavePasswordProgressLogger() {}

void BrowserSavePasswordProgressLogger::SendLog(const std::string& log) {
  log_manager_->LogSavePasswordProgress(log);
}

}  // namespace password_manager
