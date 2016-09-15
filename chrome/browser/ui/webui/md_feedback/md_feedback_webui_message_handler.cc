// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/md_feedback/md_feedback_webui_message_handler.h"

#include "base/bind.h"
#include "base/values.h"
#include "chrome/browser/feedback/feedback_dialog_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/md_feedback/md_feedback_ui.h"
#include "components/signin/core/browser/signin_manager.h"

namespace {

// Message names.
constexpr char kRequestData[] = "requestData";

// JS function names.
constexpr char kSetData[] = "Feedback.UI.setData";

}  // namespace

MdFeedbackWebUIMessageHandler::MdFeedbackWebUIMessageHandler(
    MdFeedbackUI* md_feedback_ui)
    : md_feedback_ui_(md_feedback_ui) {
  DCHECK(md_feedback_ui_);
}

MdFeedbackWebUIMessageHandler::~MdFeedbackWebUIMessageHandler() {}

void MdFeedbackWebUIMessageHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      kRequestData, base::Bind(&MdFeedbackWebUIMessageHandler::OnRequestData,
                               base::Unretained(this)));
}

void MdFeedbackWebUIMessageHandler::OnRequestData(const base::ListValue* args) {
  DVLOG(1) << "OnRequestData";
  base::DictionaryValue data;

  Profile* profile = md_feedback_ui_->profile();
  // Do not launch feedback on an OTR profile.
  profile = profile->GetOriginalProfile();
  DCHECK(profile);

  SigninManagerBase* signin_manager =
      SigninManagerFactory::GetForProfile(profile);
  DCHECK(signin_manager);
  data.SetString("email", signin_manager->GetAuthenticatedAccountInfo().email);

  GURL page_url = GURL();
  Browser* browser = chrome::FindBrowserWithProfile(profile);
  if (browser) {
    page_url = chrome::GetTargetTabUrl(
        browser->session_id().id(), browser->tab_strip_model()->active_index());
  }

  data.SetString("url", page_url.spec());

  web_ui()->CallJavascriptFunctionUnsafe(kSetData, data);
}
