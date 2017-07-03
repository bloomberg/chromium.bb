// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_manager_metrics_recorder.h"

#include "base/metrics/histogram_macros.h"
#include "components/autofill/core/common/save_password_progress_logger.h"
#include "components/password_manager/core/browser/browser_save_password_progress_logger.h"
#include "url/gurl.h"

// Shorten the name to spare line breaks. The code provides enough context
// already.
typedef autofill::SavePasswordProgressLogger Logger;

namespace password_manager {

PasswordManagerMetricsRecorder::PasswordManagerMetricsRecorder(
    std::unique_ptr<ukm::UkmEntryBuilder> ukm_entry_builder)
    : ukm_entry_builder_(std::move(ukm_entry_builder)) {}

PasswordManagerMetricsRecorder::PasswordManagerMetricsRecorder(
    PasswordManagerMetricsRecorder&& that) = default;

PasswordManagerMetricsRecorder::~PasswordManagerMetricsRecorder() {
  if (user_modified_password_field_)
    RecordUkmMetric("UserModifiedPasswordField", 1);
}

PasswordManagerMetricsRecorder& PasswordManagerMetricsRecorder::operator=(
    PasswordManagerMetricsRecorder&& that) = default;

// static
std::unique_ptr<ukm::UkmEntryBuilder>
PasswordManagerMetricsRecorder::CreateUkmEntryBuilder(
    ukm::UkmRecorder* ukm_recorder,
    ukm::SourceId source_id) {
  if (!ukm_recorder)
    return nullptr;
  return ukm_recorder->GetEntryBuilder(source_id, "PageWithPassword");
}

void PasswordManagerMetricsRecorder::RecordUserModifiedPasswordField() {
  user_modified_password_field_ = true;
}

void PasswordManagerMetricsRecorder::RecordProvisionalSaveFailure(
    ProvisionalSaveFailure failure,
    const GURL& main_frame_url,
    const GURL& form_origin,
    BrowserSavePasswordProgressLogger* logger) {
  UMA_HISTOGRAM_ENUMERATION("PasswordManager.ProvisionalSaveFailure", failure,
                            MAX_FAILURE_VALUE);

  if (logger) {
    switch (failure) {
      case SAVING_DISABLED:
        logger->LogMessage(Logger::STRING_SAVING_DISABLED);
        break;
      case EMPTY_PASSWORD:
        logger->LogMessage(Logger::STRING_EMPTY_PASSWORD);
        break;
      case MATCHING_NOT_COMPLETE:
        logger->LogMessage(Logger::STRING_MATCHING_NOT_COMPLETE);
        break;
      case NO_MATCHING_FORM:
        logger->LogMessage(Logger::STRING_NO_MATCHING_FORM);
        break;
      case FORM_BLACKLISTED:
        logger->LogMessage(Logger::STRING_FORM_BLACKLISTED);
        break;
      case INVALID_FORM:
        logger->LogMessage(Logger::STRING_INVALID_FORM);
        break;
      case SYNC_CREDENTIAL:
        logger->LogMessage(Logger::STRING_SYNC_CREDENTIAL);
        break;
      case SAVING_ON_HTTP_AFTER_HTTPS:
        logger->LogSuccessiveOrigins(
            Logger::STRING_BLOCK_PASSWORD_SAME_ORIGIN_INSECURE_SCHEME,
            main_frame_url.GetOrigin(), form_origin.GetOrigin());
        break;
      case MAX_FAILURE_VALUE:
        NOTREACHED();
        return;
    }
    logger->LogMessage(Logger::STRING_DECISION_DROP);
  }
}

void PasswordManagerMetricsRecorder::RecordUkmMetric(const char* metric_name,
                                                     int64_t value) {
  if (ukm_entry_builder_)
    ukm_entry_builder_->AddMetric(metric_name, value);
}

}  // namespace password_manager
