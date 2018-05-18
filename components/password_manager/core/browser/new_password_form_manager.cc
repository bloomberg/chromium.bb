// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/new_password_form_manager.h"

#include "components/password_manager/core/browser/browser_save_password_progress_logger.h"
#include "components/password_manager/core/browser/form_fetcher_impl.h"
#include "components/password_manager/core/browser/form_parsing/ios_form_parser.h"
#include "components/password_manager/core/browser/password_form_filling.h"
#include "components/password_manager/core/browser/password_form_metrics_recorder.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_manager_driver.h"
#include "components/password_manager/core/browser/password_manager_util.h"

using autofill::FormData;

using Logger = autofill::SavePasswordProgressLogger;

namespace password_manager {

namespace {

// On iOS, id are used for field identification, whereas on other platforms
// name field is used. Set id equal to name.
// TODO(https://crbug.com/831123): Remove this method when the browser side form
// parsing is ready.
FormData PreprocessFormData(const FormData& form) {
  FormData result_form = form;
  for (auto& field : result_form.fields)
    field.id = field.name;
  return result_form;
}

// Helper function for calling form parsing and logging results if logging is
// active.
std::unique_ptr<autofill::PasswordForm> ParseFormAndMakeLogging(
    PasswordManagerClient* client,
    const FormData& form) {
  // iOS form parsing is a prototype for parsing on all platforms. Call it for
  // developing and experimentation purposes.
  // TODO(https://crbug.com/831123): Call general form parsing instead of iOS
  // one when it is ready.
  std::unique_ptr<autofill::PasswordForm> password_form =
      ParseFormData(PreprocessFormData(form), FormParsingMode::FILLING);

  if (password_manager_util::IsLoggingActive(client)) {
    BrowserSavePasswordProgressLogger logger(client->GetLogManager());
    logger.LogFormData(Logger::STRING_FORM_PARSING_INPUT, form);
    if (password_form)
      logger.LogPasswordForm(Logger::STRING_FORM_PARSING_OUTPUT,
                             *password_form);
  }
  return password_form;
}

}  // namespace

NewPasswordFormManager::NewPasswordFormManager(
    PasswordManagerClient* client,
    const base::WeakPtr<PasswordManagerDriver>& driver,
    const FormData& observed_form,
    FormFetcher* form_fetcher)
    : client_(client),
      driver_(driver),
      observed_form_(observed_form),
      owned_form_fetcher_(
          form_fetcher ? nullptr
                       : std::make_unique<FormFetcherImpl>(
                             PasswordStore::FormDigest(observed_form),
                             client_,
                             true /* should_migrate_http_passwords */,
                             true /* should_query_suppressed_https_forms */)),
      form_fetcher_(form_fetcher ? form_fetcher : owned_form_fetcher_.get()) {
  metrics_recorder_ = base::MakeRefCounted<PasswordFormMetricsRecorder>(
      client_->IsMainFrameSecure(), client_->GetUkmSourceId());
  metrics_recorder_->RecordFormSignature(CalculateFormSignature(observed_form));

  if (owned_form_fetcher_)
    owned_form_fetcher_->Fetch();
  form_fetcher_->AddConsumer(this);

  // The folloing code is for development and debugging purposes.
  // TODO(https://crbug.com/831123): remove it when NewPasswordFormManager will
  // be production ready.
  if (password_manager_util::IsLoggingActive(client_))
    ParseFormAndMakeLogging(client_, observed_form_);
}
NewPasswordFormManager::~NewPasswordFormManager() = default;

bool NewPasswordFormManager::DoesManage(const autofill::FormData& form) const {
  if (observed_form_.is_form_tag != form.is_form_tag)
    return false;
  // All unowned input elements are considered as one synthetic form.
  if (!observed_form_.is_form_tag && !form.is_form_tag)
    return true;
  return observed_form_.unique_renderer_id == form.unique_renderer_id;
}

void NewPasswordFormManager::ProcessMatches(
    const std::vector<const autofill::PasswordForm*>& non_federated,
    size_t filtered_count) {
  if (!driver_)
    return;

  // There are additional signals (server-side data) and parse results in
  // filling and saving mode might be different so it is better not to cache
  // parse result, but to parse each time again.
  std::unique_ptr<autofill::PasswordForm> observed_password_form =
      ParseFormAndMakeLogging(client_, observed_form_);
  if (!observed_password_form)
    return;

  // TODO(https://crbug.com/831123). Implement correct treating of blacklisted
  // matches.
  std::map<base::string16, const autofill::PasswordForm*> best_matches;
  std::vector<const autofill::PasswordForm*> not_best_matches;
  const autofill::PasswordForm* preferred_match = nullptr;
  password_manager_util::FindBestMatches(non_federated, &best_matches,
                                         &not_best_matches, &preferred_match);
  if (best_matches.empty())
    return;

  // TODO(https://crbug.com/831123). Implement correct treating of federated
  // matches.
  std::vector<const autofill::PasswordForm*> federated_matches;
  SendFillInformationToRenderer(
      *client_, driver_.get(), false /* is_blaclisted */,
      *observed_password_form.get(), best_matches, federated_matches,
      preferred_match, metrics_recorder_.get());
}

bool NewPasswordFormManager::SetSubmittedFormIfIsManaged(
    const autofill::FormData& submitted_form) {
  if (!DoesManage(submitted_form))
    return false;
  submitted_form_ = submitted_form;
  is_submitted_ = true;
  return true;
}

}  // namespace password_manager
