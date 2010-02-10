// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/printer_query.h"

#include "base/logging.h"
#include "base/task.h"
#include "chrome/browser/printing/print_job_worker.h"

namespace printing {

PrinterQuery::PrinterQuery() : ui_message_loop_(NULL) {
  NOTIMPLEMENTED();
}

PrinterQuery::~PrinterQuery() {
  NOTIMPLEMENTED();
}

void PrinterQuery::GetSettingsDone(const PrintSettings& new_settings,
                                   PrintingContext::Result result) {
  NOTIMPLEMENTED();
}

PrintJobWorker* PrinterQuery::DetachWorker(PrintJobWorkerOwner* new_owner) {
  NOTIMPLEMENTED();
  return NULL;
}

void PrinterQuery::GetSettings(GetSettingsAskParam ask_user_for_settings,
                               gfx::NativeWindow parent_window,
                               int expected_page_count,
                               bool has_selection,
                               CancelableTask* callback) {
  NOTIMPLEMENTED();
}

void PrinterQuery::StopWorker() {
  NOTIMPLEMENTED();
}

bool PrinterQuery::is_print_dialog_box_shown() const {
  NOTIMPLEMENTED();
  return false;
}

bool PrinterQuery::is_callback_pending() const {
  NOTIMPLEMENTED();
  return false;
}

bool PrinterQuery::is_valid() const {
  NOTIMPLEMENTED();
  return false;
}

}  // namespace printing
