// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/new_password_form_manager.h"

#include "components/password_manager/core/browser/browser_save_password_progress_logger.h"
#include "components/password_manager/core/browser/form_parsing/ios_form_parser.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_manager_util.h"

using autofill::FormData;

using Logger = autofill::SavePasswordProgressLogger;

namespace password_manager {

NewPasswordFormManager::NewPasswordFormManager(
    PasswordManagerClient* client,
    const autofill::FormData& observed_form)
    : client_(client), observed_form_(observed_form) {
  if (password_manager_util::IsLoggingActive(client_)) {
    BrowserSavePasswordProgressLogger logger(client_->GetLogManager());
    // On iOS, id are used for field identification, whereas on other platforms
    // name. Set id equal to name.
    auto mutable_observed_form = observed_form_;
    for (auto& field : mutable_observed_form.fields)
      field.id = field.name;
    logger.LogFormData(Logger::STRING_FORM_PARSING_INPUT,
                       mutable_observed_form);
    // iOS form parsing is a prototype for parsing on all platforms. Call it for
    // developing and experimentation purposes.
    // TODO(https://crbug.com/831123): Call general form parsing instead of iOS
    // one when it is ready.
    std::unique_ptr<autofill::PasswordForm> password_form =
        ParseFormData(mutable_observed_form, FormParsingMode::FILLING);
    logger.LogPasswordForm(Logger::STRING_FORM_PARSING_OUTPUT, *password_form);
  }
}
NewPasswordFormManager::~NewPasswordFormManager() = default;

bool NewPasswordFormManager::DoesManage(const autofill::FormData& form) const {
  return observed_form_.SameFormAs(form);
}

}  // namespace password_manager
