// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/chromeos/proxy_handler.h"

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/stl_util.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

namespace chromeos {

ProxyHandler::ProxyHandler() {
}

ProxyHandler::~ProxyHandler() {
}

void ProxyHandler::GetLocalizedValues(
    DictionaryValue* localized_strings) {
  DCHECK(localized_strings);
  // Proxy page - ChromeOS
  localized_strings->SetString("proxyPage",
      l10n_util::GetStringUTF16(IDS_OPTIONS_PROXY_TAB_LABEL));
  localized_strings->SetString("proxyPageTitleFormat",
     l10n_util::GetStringUTF16(IDS_PROXY_PAGE_TITLE_FORMAT));
  localized_strings->SetString("proxy_config_title",
     l10n_util::GetStringUTF16(IDS_PROXY_CONFIG_TITLE));
  localized_strings->SetString("proxyDirectInternetConnection",
     l10n_util::GetStringUTF16(IDS_PROXY_DIRECT_CONNECTION));

  localized_strings->SetString("proxyManual",
     l10n_util::GetStringUTF16(IDS_PROXY_MANUAL_CONFIG));
  localized_strings->SetString("sameProxyProtocols",
     l10n_util::GetStringUTF16(IDS_PROXY_SAME_FORALL));

  localized_strings->SetString("httpProxy",
     l10n_util::GetStringUTF16(IDS_PROXY_HTTP_PROXY));
  localized_strings->SetString("secureHttpProxy",
     l10n_util::GetStringUTF16(IDS_PROXY_HTTP_SECURE_HTTP_PROXY));
  localized_strings->SetString("ftpProxy",
     l10n_util::GetStringUTF16(IDS_PROXY_FTP_PROXY));
  localized_strings->SetString("socksHost",
     l10n_util::GetStringUTF16(IDS_PROXY_SOCKS_HOST));
  localized_strings->SetString("proxyAutomatic",
     l10n_util::GetStringUTF16(IDS_PROXY_AUTOMATIC));
  localized_strings->SetString("proxyConfigUrl",
     l10n_util::GetStringUTF16(IDS_PROXY_CONFIG_URL));
  localized_strings->SetString("advanced_proxy_config",
     l10n_util::GetStringUTF16(IDS_PROXY_ADVANCED_CONFIG));
  localized_strings->SetString("addHost",
     l10n_util::GetStringUTF16(IDS_PROXY_ADD_HOST));
  localized_strings->SetString("removeHost",
     l10n_util::GetStringUTF16(IDS_PROXY_REMOVE_HOST));
  localized_strings->SetString("proxyPort",
     l10n_util::GetStringUTF16(IDS_PROXY_PORT));
  localized_strings->SetString("proxyBypass",
     l10n_util::GetStringUTF16(IDS_PROXY_BYPASS));
  localized_strings->SetString("policyManagedPrefsBannerText",
      l10n_util::GetStringUTF16(IDS_OPTIONS_POLICY_MANAGED_PREFS));
  localized_strings->SetString("extensionManagedPrefsBannerText",
      l10n_util::GetStringUTF16(IDS_OPTIONS_EXTENSION_MANAGED_PREFS));
  localized_strings->SetString("unmodifiablePrefsBannerText",
      l10n_util::GetStringUTF16(IDS_OPTIONS_UNMODIFIABLE_PREFS));
  localized_strings->SetString("enableSharedProxiesBannerText",
      l10n_util::GetStringFUTF16(
          IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_ENABLE_SHARED_PROXIES_HINT,
          l10n_util::GetStringUTF16(IDS_OPTIONS_SETTINGS_USE_SHARED_PROXIES)));
}

void ProxyHandler::SetNetworkName(const std::string& name) {
  StringValue network(name);
  web_ui_->CallJavascriptFunction("options.ProxyOptions.setNetworkName",
                                  network);
}

}  // namespace chromeos
