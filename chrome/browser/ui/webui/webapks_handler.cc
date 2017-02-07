// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/webapks_handler.h"

#include "base/callback_forward.h"
#include "base/values.h"
#include "chrome/browser/android/shortcut_helper.h"
#include "content/public/browser/web_ui.h"

WebApksHandler::WebApksHandler() : weak_ptr_factory_(this) {}

WebApksHandler::~WebApksHandler() {}

void WebApksHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "requestWebApksInfo",
      base::Bind(&WebApksHandler::HandleRequestWebApksInfo,
                 base::Unretained(this)));
}

void WebApksHandler::HandleRequestWebApksInfo(const base::ListValue* args) {
  AllowJavascript();
  ShortcutHelper::RetrieveWebApks(base::Bind(
      &WebApksHandler::OnWebApkInfoRetrieved, weak_ptr_factory_.GetWeakPtr()));
}

void WebApksHandler::OnWebApkInfoRetrieved(
    const std::vector<WebApkInfo>& webapks_list) {
  if (!IsJavascriptAllowed())
    return;
  base::ListValue list;
  for (const auto& webapk_info : webapks_list) {
    std::unique_ptr<base::DictionaryValue> result(new base::DictionaryValue());
    result->SetString("shortName", webapk_info.short_name);
    result->SetString("packageName", webapk_info.package_name);
    result->SetInteger("shellApkVersion", webapk_info.shell_apk_version);
    result->SetInteger("versionCode", webapk_info.version_code);
    list.Append(std::move(result));
  }

  CallJavascriptFunction("returnWebApksInfo", list);
}
