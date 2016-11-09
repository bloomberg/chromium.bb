// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/welcome_win10_handler.h"

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

WelcomeWin10Handler::WelcomeWin10Handler() {
  // The check is started as early as possible because waiting for the page to
  // be fully loaded is unnecessarily wasting time.
  StartIsPinnedToTaskbarCheck();
}

WelcomeWin10Handler::~WelcomeWin10Handler() = default;

void WelcomeWin10Handler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "handleSetDefaultBrowser",
      base::Bind(&WelcomeWin10Handler::HandleSetDefaultBrowser,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "handleContinue",
      base::Bind(&WelcomeWin10Handler::HandleContinue, base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getPinnedToTaskbarState",
      base::Bind(&WelcomeWin10Handler::HandleGetPinnedToTaskbarState,
                 base::Unretained(this)));
}

void WelcomeWin10Handler::HandleGetPinnedToTaskbarState(
    const base::ListValue* args) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  AllowJavascript();

  // Save the callback id so that the result can be sent back when it is
  // available.
  bool callback_id_found = args->GetString(0, &pinned_state_callback_id_);
  DCHECK(callback_id_found);

  // Send back the result if it is already available.
  if (pinned_state_result_) {
    SendPinnedToTaskbarStateResult();
    return;
  }

  // Only wait for a small amount of time for the result. If the timer fires,
  // it will be assumed that Chrome is pinned to the taskbar.
  constexpr base::TimeDelta kPinnedToTaskbarTimeout =
      base::TimeDelta::FromMilliseconds(200);
  timer_.Start(FROM_HERE, kPinnedToTaskbarTimeout,
               base::Bind(&WelcomeWin10Handler::OnIsPinnedToTaskbarDetermined,
                          base::Unretained(this), true));
}

void WelcomeWin10Handler::HandleSetDefaultBrowser(const base::ListValue* args) {
  // The worker owns itself.
  (new shell_integration::DefaultBrowserWorker(
       shell_integration::DefaultWebClientWorkerCallback()))
      ->StartSetAsDefault();
}

void WelcomeWin10Handler::HandleContinue(const base::ListValue* args) {
  web_ui()->GetWebContents()->GetController().LoadURL(
      GURL(chrome::kChromeUINewTabURL), content::Referrer(),
      ui::PageTransition::PAGE_TRANSITION_LINK, std::string());
}

void WelcomeWin10Handler::StartIsPinnedToTaskbarCheck() {
  // Start the utility process that will handle the IsPinnedToTaskbar() call.
  client_ =
      base::MakeUnique<content::UtilityProcessMojoClient<mojom::ShellHandler>>(
          l10n_util::GetStringUTF16(IDS_UTILITY_PROCESS_SHELL_HANDLER_NAME));

  // Assume that Chrome is pinned to the taskbar if an error occurs.
  client_->set_error_callback(
      base::Bind(&WelcomeWin10Handler::OnIsPinnedToTaskbarDetermined,
                 base::Unretained(this), true));
  client_->set_disable_sandbox();
  client_->Start();

  client_->service()->IsPinnedToTaskbar(base::Bind(
      &WelcomeWin10Handler::OnIsPinnedToTaskbarResult, base::Unretained(this)));
}

void WelcomeWin10Handler::OnIsPinnedToTaskbarResult(bool succeeded,
                                                    bool is_pinned_to_taskbar) {
  OnIsPinnedToTaskbarDetermined(succeeded && is_pinned_to_taskbar);
}

void WelcomeWin10Handler::OnIsPinnedToTaskbarDetermined(
    bool is_pinned_to_taskbar) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Reset the client and the timer to make sure this function only gets called
  // once.
  client_.reset();
  timer_.Stop();

  pinned_state_result_ = is_pinned_to_taskbar;

  // If the page already called getPinnedToTaskbarState(), the result can be
  // sent back.
  if (!pinned_state_callback_id_.empty())
    SendPinnedToTaskbarStateResult();
}

void WelcomeWin10Handler::SendPinnedToTaskbarStateResult() {
  ResolveJavascriptCallback(
      base::StringValue(pinned_state_callback_id_),
      base::FundamentalValue(pinned_state_result_.value()));
}
