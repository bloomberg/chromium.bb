// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/remoting/remoting_options_handler.h"

#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/common/remoting/chromoting_host_info.h"
#include "chrome/browser/dom_ui/dom_ui.h"
#include "chrome/browser/service/service_process_control_manager.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace remoting {

RemotingOptionsHandler::RemotingOptionsHandler()
    : dom_ui_(NULL),
      process_control_(NULL) {
}

RemotingOptionsHandler::~RemotingOptionsHandler() {
  if (process_control_)
    process_control_->RemoveMessageHandler(this);
}

void RemotingOptionsHandler::Init(DOMUI* dom_ui) {
  dom_ui_ = dom_ui;

  process_control_ =
      ServiceProcessControlManager::GetInstance()->GetProcessControl(
          dom_ui_->GetProfile());
  process_control_->AddMessageHandler(this);

  if (!process_control_->RequestRemotingHostStatus()) {
    // Assume that host is not started if we can't request status.
    SetStatus(false, "");
  }
}

// ServiceProcessControl::MessageHandler interface
void RemotingOptionsHandler::OnRemotingHostInfo(
    const remoting::ChromotingHostInfo& host_info) {
  SetStatus(host_info.enabled, host_info.login);
}

void RemotingOptionsHandler::SetStatus(
    bool enabled, const std::string& login) {
  string16 status;
  if (enabled) {
    status = l10n_util::GetStringFUTF16(IDS_REMOTING_STATUS_ENABLED_TEXT,
                                        UTF8ToUTF16(login));
  } else {
    status = l10n_util::GetStringUTF16(IDS_REMOTING_STATUS_DISABLED_TEXT);
  }

  FundamentalValue enabled_value(enabled);
  StringValue status_value(status);
  dom_ui_->CallJavascriptFunction(L"options.AdvancedOptions.SetRemotingStatus",
                                  enabled_value, status_value);
}

}  // namespace remoting
