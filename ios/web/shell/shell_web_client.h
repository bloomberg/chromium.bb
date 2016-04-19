// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_SHELL_SHELL_WEB_CLIENT_H_
#define IOS_WEB_SHELL_SHELL_WEB_CLIENT_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "ios/web/public/web_client.h"

namespace web {
class ShellBrowserState;
class ShellWebMainParts;

class ShellWebClient : public WebClient {
 public:
  ShellWebClient();
  ~ShellWebClient() override;

  // WebClient implementation.
  WebMainParts* CreateWebMainParts() override;
  std::string GetProduct() const override;
  std::string GetUserAgent(bool desktop_user_agent) const override;
  void AllowCertificateError(
      WebState* web_state,
      int cert_error,
      const net::SSLInfo& ssl_info,
      const GURL& request_url,
      bool overridable,
      const base::Callback<void(bool)>& callback) override;

  ShellBrowserState* browser_state() const;

 private:
  std::unique_ptr<ShellWebMainParts> web_main_parts_;

  DISALLOW_COPY_AND_ASSIGN(ShellWebClient);
};

}  // namespace web

#endif  // IOS_WEB_SHELL_SHELL_WEB_CLIENT_H_
