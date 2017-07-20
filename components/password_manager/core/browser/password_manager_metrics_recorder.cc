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

// URL Keyed Metrics.
const char kUkmUserModifiedPasswordField[] = "UserModifiedPasswordField";
const char kUkmProvisionalSaveFailure[] = "ProvisionalSaveFailure";

PasswordManagerMetricsRecorder::PasswordManagerMetricsRecorder(
    ukm::UkmRecorder* ukm_recorder,
    ukm::SourceId source_id,
    const GURL& main_frame_url)
    : ukm_recorder_(ukm_recorder),
      source_id_(source_id),
      main_frame_url_(main_frame_url),
      ukm_entry_builder_(
          ukm_recorder
              ? ukm_recorder->GetEntryBuilder(source_id, "PageWithPassword")
              : nullptr) {}

PasswordManagerMetricsRecorder::PasswordManagerMetricsRecorder(
    PasswordManagerMetricsRecorder&& that) noexcept = default;

PasswordManagerMetricsRecorder::~PasswordManagerMetricsRecorder() {
  if (user_modified_password_field_)
    RecordUkmMetric(kUkmUserModifiedPasswordField, 1);

  // Bind |main_frame_url_| to |source_id_| directly before sending the content
  // of |ukm_recorder_| to ensure that the binding has not been purged already.
  if (ukm_recorder_)
    ukm_recorder_->UpdateSourceURL(source_id_, main_frame_url_);
}

PasswordManagerMetricsRecorder& PasswordManagerMetricsRecorder::operator=(
    PasswordManagerMetricsRecorder&& that) = default;

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
  RecordUkmMetric(kUkmProvisionalSaveFailure, static_cast<int64_t>(failure));

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
