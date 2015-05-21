// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/history_login_handler.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/profile_info_watcher.h"
#include "content/public/browser/web_ui.h"

HistoryLoginHandler::HistoryLoginHandler() {}
HistoryLoginHandler::~HistoryLoginHandler() {}

void HistoryLoginHandler::RegisterMessages() {
  profile_info_watcher_.reset(new ProfileInfoWatcher(
      Profile::FromWebUI(web_ui()),
      base::Bind(&HistoryLoginHandler::ProfileInfoChanged,
                 base::Unretained(this))));

  web_ui()->RegisterMessageCallback("otherDevicesInitialized",
      base::Bind(&HistoryLoginHandler::HandleOtherDevicesInitialized,
                 base::Unretained(this)));
}

void HistoryLoginHandler::HandleOtherDevicesInitialized(
    const base::ListValue* /*args*/) {
  ProfileInfoChanged();
}

void HistoryLoginHandler::ProfileInfoChanged() {
  bool signed_in = !profile_info_watcher_->GetAuthenticatedUsername().empty();
  web_ui()->CallJavascriptFunction(
      "updateSignInState", base::FundamentalValue(signed_in));
}
