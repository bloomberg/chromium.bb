// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_BASIC_HTTP_USER_AGENT_SETTINGS_H_
#define CHROME_BROWSER_NET_BASIC_HTTP_USER_AGENT_SETTINGS_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "net/url_request/http_user_agent_settings.h"

// An implementation of |HttpUserAgentSettings| that provides fixed values for
// the HTTP headers Accept-Language and Accept-Charset and uses
// |content::GetUserAgent| to provide the HTTP User-Agent header value.
class BasicHttpUserAgentSettings : public net::HttpUserAgentSettings {
 public:
  BasicHttpUserAgentSettings(const std::string& accept_language,
                             const std::string& accept_charset);
  virtual ~BasicHttpUserAgentSettings();

  // HttpUserAgentSettings implementation
  virtual std::string GetAcceptLanguage() const OVERRIDE;
  virtual std::string GetAcceptCharset() const OVERRIDE;
  virtual std::string GetUserAgent(const GURL& url) const OVERRIDE;

 private:
  const std::string accept_language_;
  const std::string accept_charset_;

  DISALLOW_COPY_AND_ASSIGN(BasicHttpUserAgentSettings);
};

#endif  // CHROME_BROWSER_NET_BASIC_HTTP_USER_AGENT_SETTINGS_H_

