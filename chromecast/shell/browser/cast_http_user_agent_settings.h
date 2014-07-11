// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_SHELL_BROWSER_CAST_HTTP_USER_AGENT_SETTINGS_H_
#define CHROMECAST_SHELL_BROWSER_CAST_HTTP_USER_AGENT_SETTINGS_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/prefs/pref_member.h"
#include "net/url_request/http_user_agent_settings.h"

namespace chromecast {
namespace shell {

class CastHttpUserAgentSettings : public net::HttpUserAgentSettings {
 public:
  CastHttpUserAgentSettings();
  virtual ~CastHttpUserAgentSettings();

  // net::HttpUserAgentSettings implementation:
  virtual std::string GetAcceptLanguage() const OVERRIDE;
  virtual std::string GetUserAgent() const OVERRIDE;

 private:
  mutable std::string accept_language_;

  DISALLOW_COPY_AND_ASSIGN(CastHttpUserAgentSettings);
};

}  // namespace shell
}  // namespace chromecast
#endif  // CHROMECAST_SHELL_BROWSER_CAST_HTTP_USER_AGENT_SETTINGS_H_
