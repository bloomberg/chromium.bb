// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_REMOTING_SETUP_FLOW_REGISTER_STEP_H_
#define CHROME_BROWSER_REMOTING_SETUP_FLOW_REGISTER_STEP_H_

#include "chrome/browser/remoting/directory_add_request.h"
#include "chrome/browser/remoting/setup_flow.h"
#include "chrome/common/net/gaia/gaia_auth_consumer.h"
#include "chrome/common/net/gaia/gaia_auth_fetcher.h"

namespace remoting {

// Implementation of host registration step for remoting setup flow.
class SetupFlowRegisterStep : public SetupFlowStepBase {
 public:
  SetupFlowRegisterStep();
  virtual ~SetupFlowRegisterStep();

  // SetupFlowStep implementation.
  virtual void HandleMessage(const std::string& message,
                             const Value* arg);
  virtual void Cancel();

 protected:
  virtual void DoStart();

  // This methods is called when are sure remoting is enabled.
  void SetRemotingEnabled();
  void OnRequestDone(DirectoryAddRequest::Result result,
                     const std::string& error_message);

 private:
  scoped_ptr<DirectoryAddRequest> request_;

  DISALLOW_COPY_AND_ASSIGN(SetupFlowRegisterStep);
};

class SetupFlowRegisterErrorStep : public SetupFlowErrorStepBase {
 public:
  SetupFlowRegisterErrorStep();
  virtual ~SetupFlowRegisterErrorStep();

 protected:
  virtual string16 GetErrorMessage();
  virtual void Retry();

 private:
  DISALLOW_COPY_AND_ASSIGN(SetupFlowRegisterErrorStep);
};

}  // namespace remoting

#endif  // CHROME_BROWSER_REMOTING_SETUP_FLOW_REGISTER_STEP_H_
