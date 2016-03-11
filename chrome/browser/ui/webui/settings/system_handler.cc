// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/system_handler.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "content/public/browser/web_ui.h"

#if !defined(OS_CHROMEOS)
#include "chrome/browser/ui/webui/settings_utils.h"
#endif

namespace settings {

SystemHandler::SystemHandler() {}

SystemHandler::~SystemHandler() {}

void SystemHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback("changeProxySettings",
      base::Bind(&SystemHandler::HandleChangeProxySettings,
                 base::Unretained(this)));
}

void SystemHandler::HandleChangeProxySettings(const base::ListValue* /*args*/) {
  base::RecordAction(base::UserMetricsAction("Options_ShowProxySettings"));
#if defined(OS_CHROMEOS)
  NOTREACHED();
#else
  settings_utils::ShowNetworkProxySettings(web_ui()->GetWebContents());
#endif
}

}  // namespace settings
