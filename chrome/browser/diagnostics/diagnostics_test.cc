// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/diagnostics/diagnostics_test.h"

#include "base/files/file_path.h"
#include "base/path_service.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"

namespace diagnostics {

DiagnosticsTest::DiagnosticsTest(const std::string& id,
                                 const std::string& title)
    : id_(id),
      title_(title),
      outcome_code_(-1),
      result_(DiagnosticsModel::TEST_NOT_RUN) {}

DiagnosticsTest::~DiagnosticsTest() {}

bool DiagnosticsTest::Execute(DiagnosticsModel::Observer* observer,
                              DiagnosticsModel* model,
                              size_t index) {
  start_time_ = base::Time::Now();
  result_ = DiagnosticsModel::TEST_RUNNING;
  bool keep_going = ExecuteImpl(observer);
  if (observer)
    observer->OnTestFinished(index, model);
  return keep_going;
}

bool DiagnosticsTest::Recover(DiagnosticsModel::Observer* observer,
                              DiagnosticsModel* model,
                              size_t index) {
  result_ = DiagnosticsModel::RECOVERY_RUNNING;
  bool keep_going = RecoveryImpl(observer);
  result_ = keep_going ? DiagnosticsModel::RECOVERY_OK
                       : DiagnosticsModel::RECOVERY_FAIL_STOP;
  if (observer)
    observer->OnRecoveryFinished(index, model);
  return keep_going;
}

void DiagnosticsTest::RecordOutcome(int outcome_code,
                                    const std::string& additional_info,
                                    DiagnosticsModel::TestResult result) {
  end_time_ = base::Time::Now();
  outcome_code_ = outcome_code;
  additional_info_ = additional_info;
  result_ = result;
}

// static
base::FilePath DiagnosticsTest::GetUserDefaultProfileDir() {
  base::FilePath path;
  if (!PathService::Get(chrome::DIR_USER_DATA, &path))
    return base::FilePath();
  return path.AppendASCII(chrome::kInitialProfile);
}

std::string DiagnosticsTest::GetId() const { return id_; }

std::string DiagnosticsTest::GetTitle() const { return title_; }

DiagnosticsModel::TestResult DiagnosticsTest::GetResult() const {
  return result_;
}

int DiagnosticsTest::GetOutcomeCode() const { return outcome_code_; }

std::string DiagnosticsTest::GetAdditionalInfo() const {
  return additional_info_;
}

base::Time DiagnosticsTest::GetStartTime() const { return start_time_; }

base::Time DiagnosticsTest::GetEndTime() const { return end_time_; }

bool DiagnosticsTest::RecoveryImpl(DiagnosticsModel::Observer* observer) {
  return true;
};


}  // namespace diagnostics
