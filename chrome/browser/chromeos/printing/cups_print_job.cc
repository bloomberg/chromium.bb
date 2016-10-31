// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/printing/cups_print_job.h"

#include "base/strings/string_number_conversions.h"

namespace chromeos {

CupsPrintJob::CupsPrintJob(const Printer& printer,
                           int job_id,
                           const std::string& document_title,
                           int total_page_number)
    : printer_(printer),
      job_id_(job_id),
      document_title_(document_title),
      total_page_number_(total_page_number) {}

CupsPrintJob::~CupsPrintJob() {}

std::string CupsPrintJob::GetUniqueId() const {
  return printer_.id() + base::IntToString(job_id_);
}

}  // namespace chromeos
