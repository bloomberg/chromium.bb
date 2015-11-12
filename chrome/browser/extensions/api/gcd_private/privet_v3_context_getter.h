// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_GCD_PRIVATE_PRIVET_V3_CONTEXT_GETTER_H_
#define CHROME_BROWSER_EXTENSIONS_API_GCD_PRIVATE_PRIVET_V3_CONTEXT_GETTER_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "net/url_request/url_request_context_getter.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace net {
class CertVerifier;
class URLRequestContext;
struct SHA256HashValue;
}

namespace extensions {

class PrivetV3ContextGetter : public net::URLRequestContextGetter {
 public:
  PrivetV3ContextGetter(
      const scoped_refptr<base::SingleThreadTaskRunner>& net_task_runner,
      const net::SHA256HashValue& certificate_fingerprint);

  net::URLRequestContext* GetURLRequestContext() override;

  scoped_refptr<base::SingleThreadTaskRunner> GetNetworkTaskRunner()
      const override;

 protected:
  ~PrivetV3ContextGetter() override;

 private:
  scoped_ptr<net::CertVerifier> verifier_;
  scoped_ptr<net::URLRequestContext> context_;
  scoped_refptr<base::SingleThreadTaskRunner> net_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(PrivetV3ContextGetter);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_GCD_PRIVATE_PRIVET_V3_CONTEXT_GETTER_H_
