// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "components/policy/policy_export.h"
#include "net/url_request/static_http_user_agent_settings.h"
#include "net/url_request/url_request_context_getter.h"

#ifndef COMPONENTS_POLICY_CORE_COMMON_CLOUD_USER_POLICY_REQUEST_CONTEXT_H_
#define COMPONENTS_POLICY_CORE_COMMON_CLOUD_USER_POLICY_REQUEST_CONTEXT_H_

namespace net {
class HttpNetworkLayer;
}

namespace policy {

class POLICY_EXPORT UserPolicyRequestContext
    : public net::URLRequestContextGetter {
 public:
  UserPolicyRequestContext(
      scoped_refptr<net::URLRequestContextGetter> user_context_getter,
      scoped_refptr<net::URLRequestContextGetter> system_context_getter,
      const std::string& user_agent);

  // Overridden from net::URLRequestContextGetter:
  virtual net::URLRequestContext* GetURLRequestContext() OVERRIDE;
  virtual scoped_refptr<base::SingleThreadTaskRunner>
      GetNetworkTaskRunner() const OVERRIDE;

 protected:
  virtual ~UserPolicyRequestContext();

 private:
  scoped_refptr<net::URLRequestContextGetter> user_context_getter_;
  scoped_refptr<net::URLRequestContextGetter> system_context_getter_;

  // The lazy-initialized URLRequestContext associated with this getter.
  scoped_ptr<net::URLRequestContext> context_;

  // HttpNetworkLayer associated with |context_|.
  scoped_ptr<net::HttpNetworkLayer> http_transaction_factory_;

  net::StaticHttpUserAgentSettings http_user_agent_settings_;
  DISALLOW_COPY_AND_ASSIGN(UserPolicyRequestContext);
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_CLOUD_USER_POLICY_REQUEST_CONTEXT_H_
