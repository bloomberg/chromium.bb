// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"

#include "content/public/browser/web_ui.h"

namespace settings {

SettingsPageUIHandler::SettingsPageUIHandler() {}

SettingsPageUIHandler::~SettingsPageUIHandler() {}

void SettingsPageUIHandler::ResolveJavascriptCallback(
    const base::Value& callback_id,
    const base::Value& response) {
  // cr.webUIResponse is a global JS function exposed from cr.js.
  CallJavascriptFunction("cr.webUIResponse", callback_id,
                         base::FundamentalValue(true), response);
}

void SettingsPageUIHandler::RejectJavascriptCallback(
    const base::Value& callback_id,
    const base::Value& response) {
  // cr.webUIResponse is a global JS function exposed from cr.js.
  CallJavascriptFunction("cr.webUIResponse", callback_id,
                         base::FundamentalValue(false), response);
}

}  // namespace settings
