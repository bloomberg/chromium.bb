// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_BROWSER_SHELL_ACCESS_TOKEN_STORE_H_
#define CONTENT_SHELL_BROWSER_SHELL_ACCESS_TOKEN_STORE_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "device/geolocation/access_token_store.h"
#include "net/url_request/url_request_context_getter.h"

namespace content {
class ShellBrowserContext;

// Dummy access token store used to initialise the network location provider.
class ShellAccessTokenStore : public device::AccessTokenStore {
 public:
  explicit ShellAccessTokenStore(
      content::ShellBrowserContext* shell_browser_context);

 private:
  ~ShellAccessTokenStore() override;

  void GetRequestContextOnUIThread(
      content::ShellBrowserContext* shell_browser_context);
  void RespondOnOriginatingThread(const LoadAccessTokensCallback& callback);

  // AccessTokenStore
  void LoadAccessTokens(const LoadAccessTokensCallback& callback) override;

  void SaveAccessToken(const GURL& server_url,
                       const base::string16& access_token) override;

  content::ShellBrowserContext* shell_browser_context_;
  scoped_refptr<net::URLRequestContextGetter> system_request_context_;

  DISALLOW_COPY_AND_ASSIGN(ShellAccessTokenStore);
};

}  // namespace content

#endif  // CONTENT_SHELL_BROWSER_SHELL_ACCESS_TOKEN_STORE_H_
