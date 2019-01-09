// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CLIENT_HINTS_CLIENT_HINTS_H_
#define CHROME_BROWSER_CLIENT_HINTS_CLIENT_HINTS_H_

#include <memory>

#include "base/memory/ref_counted.h"
#include "base/optional.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/client_hints_controller_delegate.h"

class GURL;

namespace content {
class BrowserContext;
}

namespace net {
class HttpRequestHeaders;
}

namespace client_hints {

namespace internal {

// Returns |rtt| after adding host-specific random noise, and rounding it as
// per the NetInfo spec to improve privacy.
unsigned long RoundRtt(const std::string& host,
                       const base::Optional<base::TimeDelta>& rtt);

// Returns downlink (in Mbps) after adding host-specific random noise to
// |downlink_kbps| (which is in Kbps), and rounding it as per the NetInfo spec
// to improve privacy.
double RoundKbpsToMbps(const std::string& host,
                       const base::Optional<int32_t>& downlink_kbps);

}  // namespace internal

class ClientHints : public KeyedService,
                    public content::ClientHintsControllerDelegate {
 public:
  explicit ClientHints(content::BrowserContext* context);
  ~ClientHints() override;

  // content::ClientHintsControllerDelegate:
  void GetAdditionalNavigationRequestClientHintsHeaders(
      const GURL& url,
      net::HttpRequestHeaders* additional_headers) override;

 private:
  content::BrowserContext* context_;

  DISALLOW_COPY_AND_ASSIGN(ClientHints);
};

}  // namespace client_hints

#endif  // CHROME_BROWSER_CLIENT_HINTS_CLIENT_HINTS_H_
