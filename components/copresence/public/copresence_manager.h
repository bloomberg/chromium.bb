// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COPRESENCE_PUBLIC_COPRESENCE_MANAGER_H_
#define COMPONENTS_COPRESENCE_PUBLIC_COPRESENCE_MANAGER_H_

#include "base/memory/scoped_ptr.h"
#include "components/copresence/public/copresence_delegate.h"

namespace copresence {

class ReportRequest;

// The CopresenceManager class is the central interface for Copresence
// functionality. This class handles all the initialization and delegation
// of copresence tasks. Any user of copresence only needs to interact
// with this class.
class CopresenceManager {
 public:
  CopresenceManager() {}
  virtual ~CopresenceManager() {}

  // This method will execute a report request. Each report request can have
  // multiple (un)publishes, (un)subscribes. This will ensure that once the
  // manager is initialized, it sends all request to the server and handles
  // the response. If an error is encountered, the status callback is used
  // to relay it to the requester.
  virtual void ExecuteReportRequest(ReportRequest request,
                                    const std::string& app_id,
                                    const StatusCallback& callback) = 0;

  // Factory method for CopresenceManagers. The delegate is owned
  // by the caller, and must outlive the manager.
  static scoped_ptr<CopresenceManager> Create(CopresenceDelegate* delegate);

 private:

  DISALLOW_COPY_AND_ASSIGN(CopresenceManager);
};

}  // namespace copresence

#endif  // COMPONENTS_COPRESENCE_PUBLIC_COPRESENCE_MANAGER_H_
