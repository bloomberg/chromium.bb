// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/print_job_manager.h"

#include "base/logging.h"
#include "chrome/browser/printing/print_job.h"
#include "chrome/browser/printing/printer_query.h"
#include "chrome/common/notification_type.h"

namespace printing {

PrintJobManager::PrintJobManager() {
  NOTIMPLEMENTED();
}

PrintJobManager::~PrintJobManager() {
  NOTIMPLEMENTED();
}

void PrintJobManager::OnQuit() {
  NOTIMPLEMENTED();
}

void PrintJobManager::QueuePrinterQuery(PrinterQuery* job) {
  NOTIMPLEMENTED();
}

void PrintJobManager::PopPrinterQuery(int document_cookie,
                                      scoped_refptr<PrinterQuery>* job) {
  NOTIMPLEMENTED();
}

void PrintJobManager::Observe(NotificationType type,
                              const NotificationSource& source,
                              const NotificationDetails& details) {
  NOTIMPLEMENTED();
}

}  // namespace printing
