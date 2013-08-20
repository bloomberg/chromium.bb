// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CLOUD_PRINT_GCP20_PROTOTYPE_LOCAL_PRINT_JOB_H_
#define CLOUD_PRINT_GCP20_PROTOTYPE_LOCAL_PRINT_JOB_H_

#include <string>

struct LocalPrintJob {
  enum CreateResult {
    CREATE_SUCCESS,
    CREATE_INVALID_TICKET,
    CREATE_PRINTER_BUSY,
    CREATE_PRINTER_ERROR,
  };

  enum SaveResult {
    SAVE_SUCCESS,
    SAVE_INVALID_PRINT_JOB,
    SAVE_INVALID_DOCUMENT_TYPE,
    SAVE_INVALID_DOCUMENT,
    SAVE_DOCUMENT_TOO_LARGE,
    SAVE_PRINTER_BUSY,
    SAVE_PRINTER_ERROR,
  };

  enum State {
    STATE_DRAFT,
    STATE_ABORTED,
    STATE_DONE,
  };

  struct Info {
    Info();
    ~Info();

    State state;
    int expires_in;
  };

  LocalPrintJob();
  ~LocalPrintJob();

  std::string user_name;
  std::string client_name;
  std::string job_name;
  std::string content;
  std::string content_type;
  bool offline;
};

#endif  // CLOUD_PRINT_GCP20_PROTOTYPE_LOCAL_PRINT_JOB_H_

