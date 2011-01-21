// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/remoting/setup_flow_get_status_step.h"

#include "chrome/browser/remoting/setup_flow_register_step.h"
#include "chrome/browser/service/service_process_control.h"
#include "chrome/browser/service/service_process_control_manager.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace remoting {

SetupFlowGetStatusStep::SetupFlowGetStatusStep()
    : ALLOW_THIS_IN_INITIALIZER_LIST(task_factory_(this)),
      status_requested_(false) {
}

SetupFlowGetStatusStep::~SetupFlowGetStatusStep() {
  if (process_control_)
    process_control_->RemoveMessageHandler(this);
}

void SetupFlowGetStatusStep::HandleMessage(const std::string& message,
                                           const Value* arg) {
}

void SetupFlowGetStatusStep::Cancel() {
  if (process_control_)
    process_control_->RemoveMessageHandler(this);
}

void SetupFlowGetStatusStep::OnRemotingHostInfo(
    const remoting::ChromotingHostInfo& host_info) {
  if (status_requested_) {
    flow()->context()->host_info = host_info;
    status_requested_ = false;
    FinishStep(new SetupFlowRegisterStep());
  }
}

void SetupFlowGetStatusStep::DoStart() {
  flow()->dom_ui()->CallJavascriptFunction(L"showSettingUp");

  process_control_ =
      ServiceProcessControlManager::GetInstance()->GetProcessControl(
          flow()->profile());
  if (!process_control_->is_connected()) {
    LaunchServiceProcess();
  } else {
    RequestStatus();
  }
}

void SetupFlowGetStatusStep::LaunchServiceProcess() {
  Task* done_task = task_factory_.NewRunnableMethod(
      &SetupFlowGetStatusStep::OnServiceProcessLaunched);
  process_control_->Launch(done_task, done_task);
}

void SetupFlowGetStatusStep::OnServiceProcessLaunched() {
  if (!process_control_->is_connected()) {
    // Failed to start service process.
    FinishStep(new SetupFlowGetStatusErrorStep());
  } else {
    RequestStatus();
  }
}

void SetupFlowGetStatusStep::RequestStatus() {
  DCHECK(!status_requested_);

  if (!process_control_->RequestRemotingHostStatus()) {
    FinishStep(new SetupFlowGetStatusErrorStep());
    return;
  }

  status_requested_ = true;
  process_control_->AddMessageHandler(this);
}

SetupFlowGetStatusErrorStep::SetupFlowGetStatusErrorStep() { }
SetupFlowGetStatusErrorStep::~SetupFlowGetStatusErrorStep() { }

string16 SetupFlowGetStatusErrorStep::GetErrorMessage() {
  return l10n_util::GetStringUTF16(IDS_REMOTING_SERVICE_PROCESS_FAILED_MESSAGE);
}

void SetupFlowGetStatusErrorStep::Retry() {
  FinishStep(new SetupFlowGetStatusStep());
}

}  // namespace remoting
