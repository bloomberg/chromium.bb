// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dns_prefetch/renderer/prescient_networking_dispatcher.h"

#include "base/logging.h"

namespace dns_prefetch {

PrescientNetworkingDispatcher::PrescientNetworkingDispatcher() {
}

PrescientNetworkingDispatcher::~PrescientNetworkingDispatcher() {
}

void PrescientNetworkingDispatcher::prefetchDNS(
    const blink::WebString& hostname) {
  VLOG(2) << "Prefetch DNS: " << hostname.utf8();
  if (hostname.isEmpty())
    return;

  std::string hostname_utf8 = hostname.utf8();
  net_predictor_.Resolve(hostname_utf8.data(), hostname_utf8.length());
}

}  // namespace dns_prefetch
