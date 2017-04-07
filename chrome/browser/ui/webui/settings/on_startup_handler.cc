// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/on_startup_handler.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "chrome/browser/extensions/settings_api_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/settings_utils.h"
#include "content/public/browser/web_ui.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/management_policy.h"
#include "extensions/common/extension.h"

namespace settings {

OnStartupHandler::OnStartupHandler() {}
OnStartupHandler::~OnStartupHandler() {}

void OnStartupHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "getNtpExtension", base::Bind(&OnStartupHandler::HandleGetNtpExtension,
                                    base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "validateStartupPage",
      base::Bind(&OnStartupHandler::HandleValidateStartupPage,
                 base::Unretained(this)));
}

void OnStartupHandler::HandleGetNtpExtension(const base::ListValue* args) {
  const base::Value* callback_id;
  CHECK(args->Get(0, &callback_id));

  AllowJavascript();

  Profile* profile = Profile::FromWebUI(web_ui());
  const extensions::Extension* ntp_extension =
      extensions::GetExtensionOverridingNewTabPage(profile);

  if (!ntp_extension) {
    ResolveJavascriptCallback(*callback_id, base::Value());
    return;
  }

  base::DictionaryValue dict;
  dict.SetString("id", ntp_extension->id());
  dict.SetString("name", ntp_extension->name());
  dict.SetBoolean("canBeDisabled",
                  !extensions::ExtensionSystem::Get(profile)
                       ->management_policy()
                       ->MustRemainEnabled(ntp_extension, nullptr));
  ResolveJavascriptCallback(*callback_id, dict);
}

void OnStartupHandler::HandleValidateStartupPage(const base::ListValue* args) {
  AllowJavascript();

  CHECK_EQ(args->GetSize(), 2U);

  const base::Value* callback_id;
  CHECK(args->Get(0, &callback_id));

  std::string url_string;
  CHECK(args->GetString(1, &url_string));

  bool valid = settings_utils::FixupAndValidateStartupPage(url_string, nullptr);
  ResolveJavascriptCallback(*callback_id, base::Value(valid));
}

}  // namespace settings
