// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/diagnostics/diagnostics_test.h"

#include "base/file_path.h"
#include "base/path_service.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"

DiagnosticTest::DiagnosticTest(const string16& title)
    : title_(title), result_(DiagnosticsModel::TEST_NOT_RUN) {
}

DiagnosticTest::~DiagnosticTest() {
}

bool DiagnosticTest::Execute(DiagnosticsModel::Observer* observer,
                             DiagnosticsModel* model,
                             size_t index) {
  result_ = DiagnosticsModel::TEST_RUNNING;
  observer->OnProgress(index, 0, model);
  bool keep_going = ExecuteImpl(observer);
  observer->OnFinished(index, model);
  return keep_going;
}

string16 DiagnosticTest::GetTitle() {
  return title_;
}

DiagnosticsModel::TestResult DiagnosticTest::GetResult() {
  return result_;
}

string16 DiagnosticTest::GetAdditionalInfo() {
  return additional_info_;
}

void DiagnosticTest::RecordOutcome(const string16& additional_info,
                                   DiagnosticsModel::TestResult result) {
  additional_info_ = additional_info;
  result_ = result;
}

// static
FilePath DiagnosticTest::GetUserDefaultProfileDir() {
  FilePath path;
  if (!PathService::Get(chrome::DIR_USER_DATA, &path))
    return FilePath();
  return path.AppendASCII(chrome::kNotSignedInProfile);
}
