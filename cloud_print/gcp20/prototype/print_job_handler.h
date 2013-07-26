// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CLOUD_PRINT_GCP20_PROTOTYPE_PRINT_JOB_HANDLER_H_
#define CLOUD_PRINT_GCP20_PROTOTYPE_PRINT_JOB_HANDLER_H_

#include <string>

#include "base/basictypes.h"

class PrintJobHandler {
 public:
  PrintJobHandler();
  ~PrintJobHandler();

  bool SavePrintJob(const std::string& data,
                    const std::string& ticket,
                    const std::string& job_name,
                    const std::string& title);

 private:
  DISALLOW_COPY_AND_ASSIGN(PrintJobHandler);
};

#endif  // CLOUD_PRINT_GCP20_PROTOTYPE_PRINT_JOB_HANDLER_H_

