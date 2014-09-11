// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/shell/browser/cast_http_user_agent_settings.h"

#include "base/logging.h"
#include "chromecast/shell/common/cast_content_client.h"
#include "content/public/browser/browser_thread.h"
#include "grit/chromecast_settings.h"
#include "net/http/http_util.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(OS_ANDROID)
#include "ui/base/l10n/l10n_util_android.h"
#endif  // defined(OS_ANDROID)

namespace chromecast {
namespace shell {

CastHttpUserAgentSettings::CastHttpUserAgentSettings() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
}

CastHttpUserAgentSettings::~CastHttpUserAgentSettings() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
}

std::string CastHttpUserAgentSettings::GetAcceptLanguage() const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  if (accept_language_.empty()) {
    accept_language_ = net::HttpUtil::GenerateAcceptLanguageHeader(
#if defined(OS_ANDROID)
        l10n_util::GetDefaultLocale()
#else
        l10n_util::GetStringUTF8(IDS_CHROMECAST_SETTINGS_ACCEPT_LANGUAGES)
#endif  // defined(OS_ANDROID)
        );
  }
  return accept_language_;
}

std::string CastHttpUserAgentSettings::GetUserAgent() const {
  return chromecast::shell::GetUserAgent();
}

}  // namespace shell
}  // namespace chromecast
