// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/shell/browser/cast_http_user_agent_settings.h"

#include "base/logging.h"
#include "chromecast/shell/common/cast_content_client.h"
#include "content/public/browser/browser_thread.h"
#include "net/http/http_util.h"

namespace chromecast {
namespace shell {

CastHttpUserAgentSettings::CastHttpUserAgentSettings() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
}

CastHttpUserAgentSettings::~CastHttpUserAgentSettings() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
}

std::string CastHttpUserAgentSettings::GetAcceptLanguage() const {
  accept_language_ = net::HttpUtil::GenerateAcceptLanguageHeader("en-US");
  return accept_language_;
}

std::string CastHttpUserAgentSettings::GetUserAgent() const {
  return chromecast::shell::GetUserAgent();
}

}  // namespace shell
}  // namespace chromecast
