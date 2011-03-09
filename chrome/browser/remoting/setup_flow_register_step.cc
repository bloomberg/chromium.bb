// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/remoting/setup_flow_register_step.h"

#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/remoting/setup_flow_login_step.h"
#include "chrome/browser/remoting/setup_flow_start_host_step.h"
#include "chrome/common/pref_names.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace remoting {

SetupFlowRegisterStep::SetupFlowRegisterStep() { }
SetupFlowRegisterStep::~SetupFlowRegisterStep() { }

void SetupFlowRegisterStep::HandleMessage(const std::string& message,
                                          const Value* arg) {
}

void SetupFlowRegisterStep::Cancel() {
  // Don't need to do anything here. The request will be canceled when
  // |request_| is destroyed.
}

void SetupFlowRegisterStep::DoStart() {
  flow()->web_ui()->CallJavascriptFunction("showSettingUp");

  request_.reset(new DirectoryAddRequest(
      flow()->profile()->GetRequestContext()));
  request_->AddHost(flow()->context()->host_info,
                    flow()->context()->remoting_token,
                    NewCallback(this, &SetupFlowRegisterStep::OnRequestDone));
}

void SetupFlowRegisterStep::SetRemotingEnabled() {
  flow()->profile()->GetPrefs()->SetBoolean(
      prefs::kRemotingHasSetupCompleted, true);
}

void SetupFlowRegisterStep::OnRequestDone(DirectoryAddRequest::Result result,
                                          const std::string& error_message) {
  switch (result) {
    case DirectoryAddRequest::SUCCESS:
      SetRemotingEnabled();
      FinishStep(new SetupFlowStartHostStep());
      break;
    case DirectoryAddRequest::ERROR_EXISTS:
      SetRemotingEnabled();
      LOG(INFO) << "Chromoting host is already registered.";
      FinishStep(new SetupFlowStartHostStep());
      break;
    case DirectoryAddRequest::ERROR_AUTH:
      LOG(ERROR) << "Access denied by Chromoting Directory.";
      FinishStep(new SetupFlowLoginStep(l10n_util::GetStringUTF16(
          IDS_REMOTING_REGISTRATION_ACCESS_DENIED)));
      break;
    default:
      LOG(ERROR) << "Chromoting Host registration failed: "
                 << error_message << " (" << result << ")";
      FinishStep(new SetupFlowRegisterErrorStep());
      break;
  }
}

SetupFlowRegisterErrorStep::SetupFlowRegisterErrorStep() { }
SetupFlowRegisterErrorStep::~SetupFlowRegisterErrorStep() { }

string16 SetupFlowRegisterErrorStep::GetErrorMessage() {
  return l10n_util::GetStringUTF16(IDS_REMOTING_REGISTRATION_FAILED_MESSAGE);
}

void SetupFlowRegisterErrorStep::Retry() {
  // When retrying we retry from the GetStatus step because it may be
  // necessary to start service process.
  FinishStep(new SetupFlowRegisterStep());
}

}  // namespace remoting
