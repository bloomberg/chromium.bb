// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_manager_metrics_recorder.h"

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

void PasswordManagerMetricsRecorder::RecordUkmMetric(const char* metric_name,
                                                     int64_t value) {
  if (ukm_entry_builder_)
    ukm_entry_builder_->AddMetric(metric_name, value);
}

}  // namespace password_manager
