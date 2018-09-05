// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/management_ui_handler.h"

#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/policy/profile_policy_connector_factory.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#endif  // defined(OS_CHROMEOS)

namespace {

#if defined(OS_CHROMEOS)
base::string16 GetEnterpriseDisplayDomain(
    policy::BrowserPolicyConnectorChromeOS* connector) {
  if (!connector->IsEnterpriseManaged())
    return l10n_util::GetStringUTF16(IDS_MANAGEMENT_DEVICE_NOT_MANAGED);

  std::string display_domain = connector->GetEnterpriseDisplayDomain();

  if (display_domain.empty()) {
    if (!connector->IsActiveDirectoryManaged())
      return l10n_util::GetStringUTF16(IDS_MANAGEMENT_DEVICE_MANAGED);

    display_domain = connector->GetRealm();
  }

  return l10n_util::GetStringFUTF16(IDS_MANAGEMENT_DEVICE_MANAGED_BY,
                                    base::UTF8ToUTF16(display_domain));
}
#endif  // defined(OS_CHROMEOS)

}  // namespace

ManagementUIHandler::ManagementUIHandler() {}

ManagementUIHandler::~ManagementUIHandler() {}

void ManagementUIHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "initialized",
      base::BindRepeating(&ManagementUIHandler::HandleInitialized,
                          base::Unretained(this)));
}

void ManagementUIHandler::HandleInitialized(const base::ListValue* /* args */) {
  AllowJavascript();

  ShowDeviceManagementStatus();
}

void ManagementUIHandler::ShowDeviceManagementStatus() {
#if defined(OS_CHROMEOS)
  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();

  CallJavascriptFunction("management.Page.showDeviceManagedStatus",
                         base::Value(GetEnterpriseDisplayDomain(connector)));
#endif  // defined(OS_CHROMEOS)
}
