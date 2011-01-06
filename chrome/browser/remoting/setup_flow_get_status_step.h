// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_REMOTING_SETUP_FLOW_GET_STATUS_STEP_H_
#define CHROME_BROWSER_REMOTING_SETUP_FLOW_GET_STATUS_STEP_H_

#include "chrome/browser/remoting/setup_flow.h"
#include "chrome/browser/service/service_process_control.h"
#include "chrome/common/net/gaia/gaia_auth_consumer.h"
#include "chrome/common/net/gaia/gaia_auth_fetcher.h"

namespace remoting {

// SetupFlowGetStatusStep requests current host information from the service
// process. It also starts service process if necessary.
class SetupFlowGetStatusStep : public SetupFlowStepBase,
                               public ServiceProcessControl::MessageHandler {
 public:
  SetupFlowGetStatusStep();
  virtual ~SetupFlowGetStatusStep();

  // SetupFlowStep implementation.
  virtual void HandleMessage(const std::string& message, const Value* arg);
  virtual void Cancel();

  // ServiceProcessControl::MessageHandler interface
  virtual void OnRemotingHostInfo(
      const remoting::ChromotingHostInfo& host_info);

 protected:
  virtual void DoStart();

 private:
  void LaunchServiceProcess();
  void OnServiceProcessLaunched();
  void RequestStatus();

  ScopedRunnableMethodFactory<SetupFlowGetStatusStep> task_factory_;
  ServiceProcessControl* process_control_;
  bool status_requested_;

  DISALLOW_COPY_AND_ASSIGN(SetupFlowGetStatusStep);
};

class SetupFlowGetStatusErrorStep : public SetupFlowErrorStepBase {
 public:
  SetupFlowGetStatusErrorStep();
  virtual ~SetupFlowGetStatusErrorStep();

 protected:
  virtual string16 GetErrorMessage();
  virtual void Retry();

 private:
  DISALLOW_COPY_AND_ASSIGN(SetupFlowGetStatusErrorStep);
};

}  // namespace remoting

#endif  // CHROME_BROWSER_REMOTING_SETUP_FLOW_GET_STATUS_STEP_H_
