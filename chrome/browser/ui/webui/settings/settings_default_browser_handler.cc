// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/settings_default_browser_handler.h"

#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/startup/default_browser_prompt.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_ui.h"

namespace settings {

namespace {

bool IsDisabledByPolicy(const BooleanPrefMember& pref) {
  return pref.IsManaged() && !pref.GetValue();
}

}  // namespace

DefaultBrowserHandler::DefaultBrowserHandler(content::WebUI* webui)
    : weak_ptr_factory_(this) {
  default_browser_worker_ = new shell_integration::DefaultBrowserWorker(
      base::Bind(&DefaultBrowserHandler::OnDefaultBrowserWorkerFinished,
                 weak_ptr_factory_.GetWeakPtr()));
  default_browser_policy_.Init(
      prefs::kDefaultBrowserSettingEnabled, g_browser_process->local_state(),
      base::Bind(&DefaultBrowserHandler::RequestDefaultBrowserState,
                 base::Unretained(this), nullptr));
}

DefaultBrowserHandler::~DefaultBrowserHandler() {}

void DefaultBrowserHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "SettingsDefaultBrowser.requestDefaultBrowserState",
      base::Bind(&DefaultBrowserHandler::RequestDefaultBrowserState,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "SettingsDefaultBrowser.setAsDefaultBrowser",
      base::Bind(&DefaultBrowserHandler::SetAsDefaultBrowser,
                 base::Unretained(this)));
}

void DefaultBrowserHandler::RequestDefaultBrowserState(
    const base::ListValue* /*args*/) {
  default_browser_worker_->StartCheckIsDefault();
}

void DefaultBrowserHandler::SetAsDefaultBrowser(const base::ListValue* args) {
  CHECK(!IsDisabledByPolicy(default_browser_policy_));

  base::RecordAction(base::UserMetricsAction("Options_SetAsDefaultBrowser"));
  UMA_HISTOGRAM_COUNTS("Settings.StartSetAsDefault", true);

  default_browser_worker_->StartSetAsDefault();

  // If the user attempted to make Chrome the default browser, notify
  // them when this changes.
  chrome::ResetDefaultBrowserPrompt(Profile::FromWebUI(web_ui()));
}

void DefaultBrowserHandler::OnDefaultBrowserWorkerFinished(
    shell_integration::DefaultWebClientState state) {
  if (state == shell_integration::IS_DEFAULT) {
    // Notify the user in the future if Chrome ceases to be the user's chosen
    // default browser.
    chrome::ResetDefaultBrowserPrompt(Profile::FromWebUI(web_ui()));
  }

  base::FundamentalValue is_default(state == shell_integration::IS_DEFAULT);
  base::FundamentalValue can_be_default(
      state != shell_integration::UNKNOWN_DEFAULT &&
      !IsDisabledByPolicy(default_browser_policy_) &&
      shell_integration::CanSetAsDefaultBrowser());

  web_ui()->CallJavascriptFunction("Settings.updateDefaultBrowserState",
                                   is_default, can_be_default);
}

}  // namespace settings
