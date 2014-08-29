// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/chromeos/proxy_handler.h"

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/chromeos/system/input_device_settings.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/locale_settings.h"
#include "chromeos/chromeos_constants.h"
#include "content/public/browser/web_ui.h"
#include "ui/base/l10n/l10n_util.h"

namespace chromeos {
namespace options {

ProxyHandler::ProxyHandler() {
}

ProxyHandler::~ProxyHandler() {
}

void ProxyHandler::GetLocalizedValues(
    base::DictionaryValue* localized_strings) {
  DCHECK(localized_strings);

  // Proxy page - ChromeOS
  static OptionsStringResource resources[] = {
    { "proxyPage", IDS_OPTIONS_PROXY_TAB_LABEL },
    { "proxyDirectInternetConnection", IDS_PROXY_DIRECT_CONNECTION },
    { "proxyManual", IDS_PROXY_MANUAL_CONFIG },
    { "sameProxyProtocols", IDS_PROXY_SAME_FORALL },
    { "httpProxy", IDS_PROXY_HTTP_PROXY },
    { "secureHttpProxy", IDS_PROXY_HTTP_SECURE_HTTP_PROXY },
    { "ftpProxy", IDS_PROXY_FTP_PROXY },
    { "socksHost", IDS_PROXY_SOCKS_HOST },
    { "proxyAutomatic", IDS_PROXY_AUTOMATIC },
    { "proxyUseConfigUrl", IDS_PROXY_USE_AUTOCONFIG_URL },
    { "addHost", IDS_PROXY_ADD_HOST },
    { "removeHost", IDS_PROXY_REMOVE_HOST },
    { "proxyPort", IDS_PROXY_PORT },
    { "proxyBypass", IDS_PROXY_BYPASS },
    { "proxyBannerPolicy", IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_PROXY_POLICY },
    { "proxyBannerExtension",
       IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_PROXY_EXTENSION },
    { "proxyBannerOther",
      IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_PROXY_OTHER_PRECEDE }
  };

  RegisterStrings(localized_strings, resources, arraysize(resources));

  localized_strings->SetString("proxyBannerShared",
      l10n_util::GetStringFUTF16(
          IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_PROXY_ENABLE_SHARED_HINT,
          l10n_util::GetStringUTF16(IDS_OPTIONS_SETTINGS_USE_SHARED_PROXIES)));
}

void ProxyHandler::InitializePage() {
  ::options::OptionsPageUIHandler::InitializePage();

  bool keyboard_driven_oobe =
      system::InputDeviceSettings::Get()->ForceKeyboardDrivenUINavigation();
  if (keyboard_driven_oobe) {
    web_ui()->CallJavascriptFunction(
        "DetailsInternetPage.initializeKeyboardFlow");
  }
}

}  // namespace options
}  // namespace chromeos
