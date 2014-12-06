// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DNS_PREFETCH_BROWSER_NET_MESSAGE_FILTER_H_
#define COMPONENTS_DNS_PREFETCH_BROWSER_NET_MESSAGE_FILTER_H_

#include "base/macros.h"
#include "content/public/browser/browser_message_filter.h"

namespace net {
class HostResolver;
}

namespace dns_prefetch {
struct LookupRequest;

// Simple browser-side handler for DNS prefetch requests.
// Passes prefetch requests to the provided net::HostResolver.
// Each renderer process requires its own filter.
class NetMessageFilter : public content::BrowserMessageFilter {
 public:
  explicit NetMessageFilter(net::HostResolver* host_resolver);

  // content::BrowserMessageFilter implementation:
  bool OnMessageReceived(const IPC::Message& message) override;

 private:
  ~NetMessageFilter() override;

  void OnDnsPrefetch(const LookupRequest& lookup_request);

  net::HostResolver* host_resolver_;

  DISALLOW_COPY_AND_ASSIGN(NetMessageFilter);
};

}  // namespace dns_prefetch

#endif  // COMPONENTS_DNS_PREFETCH_BROWSER_NET_MESSAGE_FILTER_H_
