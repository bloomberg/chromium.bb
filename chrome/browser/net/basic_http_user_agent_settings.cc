// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/basic_http_user_agent_settings.h"

#include "content/public/common/content_client.h"

BasicHttpUserAgentSettings::BasicHttpUserAgentSettings(
    const std::string& accept_language, const std::string& accept_charset)
    : accept_language_(accept_language),
      accept_charset_(accept_charset) {
}

BasicHttpUserAgentSettings::~BasicHttpUserAgentSettings() {
}

std::string BasicHttpUserAgentSettings::GetAcceptLanguage() const {
  return accept_language_;
}

std::string BasicHttpUserAgentSettings::GetAcceptCharset() const {
  return accept_charset_;
}

std::string BasicHttpUserAgentSettings::GetUserAgent(const GURL& url) const {
  return content::GetUserAgent(url);
}

