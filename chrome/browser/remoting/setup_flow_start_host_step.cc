// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/remoting/setup_flow_start_host_step.h"

#include "chrome/browser/remoting/setup_flow_get_status_step.h"
#include "chrome/browser/service/service_process_control.h"
#include "chrome/browser/service/service_process_control_manager.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace remoting {

SetupFlowStartHostStep::SetupFlowStartHostStep()
    : ALLOW_THIS_IN_INITIALIZER_LIST(task_factory_(this)),
      status_requested_(false) {
}

SetupFlowStartHostStep::~SetupFlowStartHostStep() {
  if (process_control_)
    process_control_->RemoveMessageHandler(this);
}

void SetupFlowStartHostStep::HandleMessage(const std::string& message,
                                           const Value* arg) {
}

void SetupFlowStartHostStep::Cancel() {
  if (process_control_)
    process_control_->RemoveMessageHandler(this);
}

void SetupFlowStartHostStep::OnRemotingHostInfo(
    const remoting::ChromotingHostInfo& host_info) {
  if (status_requested_) {
    status_requested_ = false;
    if (host_info.enabled) {
      FinishStep(new SetupFlowDoneStep());
    } else {
      FinishStep(new SetupFlowStartHostErrorStep());
    }
  }
}

void SetupFlowStartHostStep::DoStart() {
  flow()->web_ui()->CallJavascriptFunction("showSettingUp");

  process_control_ =
      ServiceProcessControlManager::GetInstance()->GetProcessControl(
          flow()->profile());
  if (!process_control_ || !process_control_->is_connected()) {
    FinishStep(new SetupFlowStartHostErrorStep());
  }

  process_control_->SetRemotingHostCredentials(flow()->context()->login,
                                               flow()->context()->talk_token);
  process_control_->EnableRemotingHost();
  RequestStatus();
}

void SetupFlowStartHostStep::RequestStatus() {
  DCHECK(!status_requested_);

  if (!process_control_->RequestRemotingHostStatus()) {
    FinishStep(new SetupFlowStartHostErrorStep());
    return;
  }

  status_requested_ = true;
  process_control_->AddMessageHandler(this);
}

SetupFlowStartHostErrorStep::SetupFlowStartHostErrorStep() { }
SetupFlowStartHostErrorStep::~SetupFlowStartHostErrorStep() { }

string16 SetupFlowStartHostErrorStep::GetErrorMessage() {
  return l10n_util::GetStringUTF16(IDS_REMOTING_SERVICE_PROCESS_FAILED_MESSAGE);
}

void SetupFlowStartHostErrorStep::Retry() {
  // When retrying we retry from the GetStatus step because it may be
  // necessary to start service process.
  FinishStep(new SetupFlowGetStatusStep());
}

}  // namespace remoting
