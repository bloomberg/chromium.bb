// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "net/url_request/static_http_user_agent_settings.h"
#include "net/url_request/url_request_context_getter.h"

#ifndef CHROME_BROWSER_POLICY_CLOUD_SYSTEM_POLICY_REQUEST_CONTEXT_H_
#define CHROME_BROWSER_POLICY_CLOUD_SYSTEM_POLICY_REQUEST_CONTEXT_H_

namespace net {
class HttpNetworkLayer;
}

namespace policy {

class SystemPolicyRequestContext
    : public net::URLRequestContextGetter {
 public:
  SystemPolicyRequestContext(
      scoped_refptr<net::URLRequestContextGetter> system_context_getter,
      const std::string& user_agent);

  // Overridden from net::URLRequestContextGetter:
  virtual net::URLRequestContext* GetURLRequestContext() OVERRIDE;
  virtual scoped_refptr<base::SingleThreadTaskRunner>
      GetNetworkTaskRunner() const OVERRIDE;

 protected:
  virtual ~SystemPolicyRequestContext();

 private:
  scoped_refptr<net::URLRequestContextGetter> system_context_getter_;

  // The lazy-initialized URLRequestContext associated with this getter.
  scoped_ptr<net::URLRequestContext> context_;

  // HttpNetworkLayer associated with |context_|.
  scoped_ptr<net::HttpNetworkLayer> http_transaction_factory_;

  net::StaticHttpUserAgentSettings http_user_agent_settings_;
  DISALLOW_COPY_AND_ASSIGN(SystemPolicyRequestContext);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_CLOUD_SYSTEM_POLICY_REQUEST_CONTEXT_H_
