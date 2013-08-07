// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/diagnostics/diagnostics_controller.h"

#include <string>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/diagnostics/diagnostics_model.h"
#include "chrome/browser/diagnostics/diagnostics_writer.h"
#include "chrome/common/chrome_switches.h"

namespace diagnostics {

DiagnosticsController* DiagnosticsController::GetInstance() {
  return Singleton<DiagnosticsController>::get();
}

DiagnosticsController::DiagnosticsController() : writer_(NULL) {}

DiagnosticsController::~DiagnosticsController() {}

const DiagnosticsModel& DiagnosticsController::GetResults() const {
  return *model_;
}

bool DiagnosticsController::HasResults() {
  return (model_.get() && model_->GetTestRunCount() > 0);
}

void DiagnosticsController::ClearResults() { model_.reset(); }

// This entry point is called from early in startup when very few things have
// been initialized, so be careful what you use.
int DiagnosticsController::Run(const CommandLine& command_line,
                               DiagnosticsWriter* writer) {
  writer_ = writer;

  model_.reset(MakeDiagnosticsModel(command_line));
  model_->RunAll(writer_);

  return 0;
}

// This entry point is called from early in startup when very few things have
// been initialized, so be careful what you use.
int DiagnosticsController::RunRecovery(const CommandLine& command_line,
                                       DiagnosticsWriter* writer) {
  if (!HasResults()) {
    if (writer) {
      writer->WriteInfoLine("No diagnostics have been run.");
      writer->OnAllRecoveryDone(model_.get());
    }
    return -1;
  }

  writer_ = writer;

  model_->RecoverAll(writer_);
  return 0;
}

}  // namespace diagnostics
