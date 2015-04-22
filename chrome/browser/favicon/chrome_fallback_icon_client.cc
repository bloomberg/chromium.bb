// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/favicon/chrome_fallback_icon_client.h"

#include "base/i18n/case_conversion.h"
#include "base/strings/utf_string_conversions.h"
#include "grit/platform_locale_settings.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace {
const char* kFallbackIconTextForIP = "IP";
}  // namespace

ChromeFallbackIconClient::ChromeFallbackIconClient() {
#if defined(OS_CHROMEOS)
  font_list_.push_back("Noto Sans");
#elif defined(OS_IOS)
  font_list_.push_back("Helvetica Neue");
#else
  font_list_.push_back(l10n_util::GetStringUTF8(IDS_SANS_SERIF_FONT_FAMILY));
#endif
}

ChromeFallbackIconClient::~ChromeFallbackIconClient() {
}

const std::vector<std::string>& ChromeFallbackIconClient::GetFontNameList()
    const {
  return font_list_;
}

// Returns a single character to represent |url|. To do this we take the first
// letter in a domain's name and make it upper case.
base::string16 ChromeFallbackIconClient::GetFallbackIconText(const GURL& url)
    const {
  if (url.is_empty())
    return base::string16();
  std::string domain = net::registry_controlled_domains::GetDomainAndRegistry(
      url, net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
  if (domain.empty()) {  // E.g., http://localhost/ or http://192.168.0.1/
    if (url.HostIsIPAddress())
      return base::ASCIIToUTF16(kFallbackIconTextForIP);
    domain = url.host();
  }
  if (domain.empty())
    return base::string16();
  // TODO(huangs): Handle non-ASCII ("xn--") domain names.
  return base::i18n::ToUpper(base::ASCIIToUTF16(domain.substr(0, 1)));
}
