// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COPRESENCE_PUBLIC_COPRESENCE_CLIENT_H_
#define COMPONENTS_COPRESENCE_PUBLIC_COPRESENCE_CLIENT_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/copresence/proto/rpcs.pb.h"
#include "components/copresence/public/copresence_client_delegate.h"

namespace net {
class URLContextGetter;
}

namespace copresence {

class CopresenceClientDelegate;
class RpcHandler;

struct PendingRequest {
  PendingRequest(const copresence::ReportRequest& report,
                 const std::string app_id,
                 const StatusCallback& callback);
  ~PendingRequest();

  copresence::ReportRequest report;
  std::string app_id;
  StatusCallback callback;
};

// The CopresenceClient class is the central interface for Copresence
// functionality. This class handles all the initialization and delegation of
// copresence tasks. Any user of copresence only needs to interact with this
// client.
class CopresenceClient : public base::SupportsWeakPtr<CopresenceClient> {
 public:
  // The delegate must outlive us.
  explicit CopresenceClient(CopresenceClientDelegate* delegate);
  virtual ~CopresenceClient();

  // This method will execute a report request. Each report request can have
  // multiple (un)publishes, (un)subscribes. This will ensure that once the
  // client is initialized, it sends all request to the server and handles
  // the response. If an error is encountered, the status callback is used
  // to relay it to the requester.
  void ExecuteReportRequest(copresence::ReportRequest request,
                            const std::string& app_id,
                            const StatusCallback& callback);

  // Called before the API (and thus the Client) is destructed.
  void Shutdown();

 private:
  void CompleteInitialization();
  void InitStepComplete(const std::string& step, bool success);

  CopresenceClientDelegate* delegate_;
  bool init_failed_;
  std::vector<PendingRequest> pending_requests_queue_;

  // TODO(rkc): This code is almost identical to what we use in feedback to
  // perform multiple blocking tasks and then run a post process method. Look
  // into refactoring it all out to a common construct, like maybe a
  // PostMultipleTasksAndReply?
  size_t pending_init_operations_;

  scoped_ptr<RpcHandler> rpc_handler_;

  DISALLOW_COPY_AND_ASSIGN(CopresenceClient);
};

}  // namespace copresence

#endif  // COMPONENTS_COPRESENCE_PUBLIC_COPRESENCE_CLIENT_H_
