// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/dom_ui/proxy_handler.h"

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/basictypes.h"
#include "base/callback.h"
#include "base/i18n/time_formatting.h"
#include "base/stl_util-inl.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/common/notification_service.h"
#include "grit/browser_resources.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "grit/theme_resources.h"

ProxyHandler::ProxyHandler() {
}

ProxyHandler::~ProxyHandler() {
}

void ProxyHandler::GetLocalizedValues(
    DictionaryValue* localized_strings) {
  DCHECK(localized_strings);
  // Proxy page - ChromeOS
  localized_strings->SetString(L"proxyPage",
      l10n_util::GetString(IDS_OPTIONS_PROXY_TAB_LABEL));
  localized_strings->SetString(L"proxy_config_title",
     l10n_util::GetString(IDS_PROXY_CONFIG_TITLE));
  localized_strings->SetString(L"proxyDirectIternetConnection",
     l10n_util::GetString(IDS_PROXY_DIRECT_CONNECTION));

  localized_strings->SetString(L"proxyManual",
     l10n_util::GetString(IDS_PROXY_MANUAL_CONFIG));
  localized_strings->SetString(L"sameProxyProtocols",
     l10n_util::GetString(IDS_PROXY_SAME_FORALL));

  localized_strings->SetString(L"httpProxy",
     l10n_util::GetString(IDS_PROXY_HTTP_PROXY));
  localized_strings->SetString(L"secureHttpProxy",
     l10n_util::GetString(IDS_PROXY_HTTP_SECURE_HTTP_PROXY));
  localized_strings->SetString(L"ftpProxy",
     l10n_util::GetString(IDS_PROXY_FTP_PROXY));
  localized_strings->SetString(L"socksHost",
     l10n_util::GetString(IDS_PROXY_SOCKS_HOST));
  localized_strings->SetString(L"proxyAutomatic",
     l10n_util::GetString(IDS_PROXY_AUTOMATIC));
  localized_strings->SetString(L"proxyConfigUrl",
     l10n_util::GetString(IDS_PROXY_CONFIG_URL));
  localized_strings->SetString(L"advanced_proxy_config",
     l10n_util::GetString(IDS_PROXY_ADVANCED_CONFIG));
  localized_strings->SetString(L"addHost",
     l10n_util::GetString(IDS_PROXY_ADD_HOST));
  localized_strings->SetString(L"removeHost",
     l10n_util::GetString(IDS_PROXY_REMOVE_HOST));
  localized_strings->SetString(L"proxyPort",
     l10n_util::GetString(IDS_PROXY_PORT));
}
