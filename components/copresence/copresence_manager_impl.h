// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COPRESENCE_COPRESENCE_MANAGER_IMPL_H_
#define COMPONENTS_COPRESENCE_COPRESENCE_MANAGER_IMPL_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/cancelable_callback.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "components/copresence/proto/rpcs.pb.h"
#include "components/copresence/public/copresence_manager.h"

namespace net {
class URLContextGetter;
}

namespace copresence {

class RpcHandler;

struct PendingRequest {
  PendingRequest(const ReportRequest& report,
                 const std::string& app_id,
                 const std::string& auth_token,
                 const StatusCallback& callback);
  ~PendingRequest();

  scoped_ptr<ReportRequest> report;
  std::string app_id;
  std::string auth_token;
  StatusCallback callback;
};

// The implementation for CopresenceManager. Responsible primarily for
// client-side initialization. The RpcHandler handles all the details
// of interacting with the server.
class CopresenceManagerImpl : public CopresenceManager {
 public:
  ~CopresenceManagerImpl() override;
  void ExecuteReportRequest(const ReportRequest& request,
                            const std::string& app_id,
                            const StatusCallback& callback) override;

 private:
  // Create managers with the CopresenceManager::Create() method.
  friend class CopresenceManager;
  CopresenceManagerImpl(CopresenceDelegate* delegate);

  void CompleteInitialization();
  void InitStepComplete(const std::string& step, bool success);

  bool init_failed_;
  ScopedVector<PendingRequest> pending_requests_queue_;

  base::CancelableCallback<void(bool)> whispernet_init_callback_;

  // TODO(rkc): This code is almost identical to what we use in feedback to
  // perform multiple blocking tasks and then run a post process method. Look
  // into refactoring it all out to a common construct, like maybe a
  // PostMultipleTasksAndReply?
  int pending_init_operations_;

  CopresenceDelegate* const delegate_;
  scoped_ptr<RpcHandler> rpc_handler_;

  DISALLOW_COPY_AND_ASSIGN(CopresenceManagerImpl);
};

}  // namespace copresence

#endif  // COMPONENTS_COPRESENCE_COPRESENCE_MANAGER_IMPL_H_
