// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_MANAGER_METRICS_RECORDER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_MANAGER_METRICS_RECORDER_H_

#include <stdint.h>

#include <memory>

#include "base/macros.h"
#include "services/metrics/public/cpp/ukm_recorder.h"

class GURL;

namespace password_manager {

// URL Keyed Metrics.

// Records a 1 for every page on which a user has modified the content of a
// password field - regardless of how many password fields a page contains or
// the user modifies.
constexpr char kUkmUserModifiedPasswordField[] = "UserModifiedPasswordField";

class BrowserSavePasswordProgressLogger;

// The pupose of this class is to record various types of metrics about the
// behavior of the PasswordManager and its interaction with the user and the
// page.
// The PasswordManagerMetricsRecorder flushes metrics on destruction. As such
// any owner needs to destroy this instance when navigations are committed.
class PasswordManagerMetricsRecorder {
 public:
  // Reasons why the password manager failed to do a provisional saving and
  // therefore did not offer the user to save a password.
  enum ProvisionalSaveFailure {
    SAVING_DISABLED,
    EMPTY_PASSWORD,
    NO_MATCHING_FORM,
    MATCHING_NOT_COMPLETE,
    FORM_BLACKLISTED,
    INVALID_FORM,
    SYNC_CREDENTIAL,
    SAVING_ON_HTTP_AFTER_HTTPS,
    MAX_FAILURE_VALUE
  };

  // |ukm_entry_builder| is the destination into which UKM metrics are recorded.
  // It may be nullptr, in which case no UKM metrics are recorded. This should
  // be created via the static CreateUkmEntryBuilder() method of this class.
  explicit PasswordManagerMetricsRecorder(
      std::unique_ptr<ukm::UkmEntryBuilder> ukm_entry_builder);
  explicit PasswordManagerMetricsRecorder(
      PasswordManagerMetricsRecorder&& that);
  ~PasswordManagerMetricsRecorder();

  PasswordManagerMetricsRecorder& operator=(
      PasswordManagerMetricsRecorder&& that);

  // Creates a UkmEntryBuilder that can be used to record metrics into the event
  // "PageWithPassword". |source_id| should be bound the the correct URL in the
  // |ukm_recorder| when this function is called.
  static std::unique_ptr<ukm::UkmEntryBuilder> CreateUkmEntryBuilder(
      ukm::UkmRecorder* ukm_recorder,
      ukm::SourceId source_id);

  // Records that the user has modified a password field on a page. This may be
  // called multiple times but a single metric will be reported.
  void RecordUserModifiedPasswordField();

  // Log failure to provisionally save a password to in the PasswordManager to
  // UMA and the |logger|.
  void RecordProvisionalSaveFailure(ProvisionalSaveFailure failure,
                                    const GURL& main_frame_url,
                                    const GURL& form_origin,
                                    BrowserSavePasswordProgressLogger* logger);

 private:
  // Records a metric into |ukm_entry_builder_| if it is not nullptr.
  void RecordUkmMetric(const char* metric_name, int64_t value);

  // Records URL keyed metrics (UKMs) and submits them on its destruction. May
  // be a nullptr in which case no recording is expected.
  std::unique_ptr<ukm::UkmEntryBuilder> ukm_entry_builder_;

  bool user_modified_password_field_ = false;

  DISALLOW_COPY_AND_ASSIGN(PasswordManagerMetricsRecorder);
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_MANAGER_METRICS_RECORDER_H_
