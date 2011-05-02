// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/remoting/remoting_options_handler.h"

#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/service/service_process_control_manager.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/remoting/chromoting_host_info.h"
#include "content/browser/webui/web_ui.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace remoting {

RemotingOptionsHandler::RemotingOptionsHandler()
    : web_ui_(NULL),
      process_control_(NULL) {
}

RemotingOptionsHandler::~RemotingOptionsHandler() {
  if (process_control_)
    process_control_->RemoveMessageHandler(this);
}

void RemotingOptionsHandler::Init(WebUI* web_ui) {
  web_ui_ = web_ui;

  process_control_ =
      ServiceProcessControlManager::GetInstance()->GetProcessControl(
          web_ui_->GetProfile());
  process_control_->AddMessageHandler(this);

  PrefService* prefs = web_ui_->GetProfile()->GetPrefs();

  if (!process_control_->RequestRemotingHostStatus()) {
    // Assume that host is not started if we can't request status.
    bool enabled = prefs->GetBoolean(prefs::kChromotingEnabled);
    bool host_enabled = prefs->GetBoolean(prefs::kChromotingHostEnabled);
    SetStatus(enabled, host_enabled, false, "");
  }
  prefs->SetBoolean(prefs::kRemotingHasSetupCompleted, false);
}

// ServiceProcessControl::MessageHandler interface
void RemotingOptionsHandler::OnRemotingHostInfo(
    const remoting::ChromotingHostInfo& host_info) {
  PrefService* prefs = web_ui_->GetProfile()->GetPrefs();
  bool enabled = prefs->GetBoolean(prefs::kChromotingEnabled);
  bool host_enabled = prefs->GetBoolean(prefs::kChromotingHostEnabled);
  DCHECK(enabled && host_enabled);

  bool host_configured = host_info.enabled;
  SetStatus(enabled, host_enabled, host_configured, host_info.login);
}

void RemotingOptionsHandler::SetStatus(
    bool enabled, bool host_enabled, bool host_configured,
    const std::string& login) {
  string16 status;
  if (enabled) {
    if (host_enabled) {
      if (host_configured) {
        status = l10n_util::GetStringFUTF16(IDS_REMOTING_STATUS_CONFIGURED_TEXT,
                                            UTF8ToUTF16(login));
      } else {
        status = l10n_util::GetStringUTF16(
            IDS_REMOTING_STATUS_HOST_ENABLED_TEXT);
      }
    } else {
      status = l10n_util::GetStringUTF16(IDS_REMOTING_STATUS_ENABLED_TEXT);
    }
  } else {
    status = l10n_util::GetStringUTF16(IDS_REMOTING_STATUS_DISABLED_TEXT);
  }

  FundamentalValue enabled_value(enabled);
  FundamentalValue configured_value(host_configured);
  StringValue status_value(status);
  web_ui_->CallJavascriptFunction(
      "options.AdvancedOptions.SetRemotingStatus",
      enabled_value, configured_value, status_value);
}

}  // namespace remoting
