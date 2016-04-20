// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/macros.h"
#include "components/policy/policy_export.h"
#include "net/url_request/static_http_user_agent_settings.h"
#include "net/url_request/url_request_context_getter.h"

#ifndef COMPONENTS_POLICY_CORE_COMMON_CLOUD_SYSTEM_POLICY_REQUEST_CONTEXT_H_
#define COMPONENTS_POLICY_CORE_COMMON_CLOUD_SYSTEM_POLICY_REQUEST_CONTEXT_H_

namespace net {
class CookieStore;
class HttpNetworkLayer;
}

namespace policy {

class POLICY_EXPORT SystemPolicyRequestContext
    : public net::URLRequestContextGetter {
 public:
  SystemPolicyRequestContext(
      scoped_refptr<net::URLRequestContextGetter> system_context_getter,
      const std::string& user_agent);

  // Overridden from net::URLRequestContextGetter:
  net::URLRequestContext* GetURLRequestContext() override;
  scoped_refptr<base::SingleThreadTaskRunner> GetNetworkTaskRunner()
      const override;

 protected:
  ~SystemPolicyRequestContext() override;

 private:
  scoped_refptr<net::URLRequestContextGetter> system_context_getter_;

  // HttpNetworkLayer associated with |context_|.
  std::unique_ptr<net::HttpNetworkLayer> http_transaction_factory_;

  std::unique_ptr<net::CookieStore> cookie_store_;

  net::StaticHttpUserAgentSettings http_user_agent_settings_;

  // The lazy-initialized URLRequestContext associated with this getter.
  std::unique_ptr<net::URLRequestContext> context_;

  DISALLOW_COPY_AND_ASSIGN(SystemPolicyRequestContext);
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_CLOUD_SYSTEM_POLICY_REQUEST_CONTEXT_H_
