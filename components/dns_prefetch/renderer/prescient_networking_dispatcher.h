// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DNS_PREFETCH_RENDERER_PRESCIENT_NETWORKING_DISPATCHER_H_
#define COMPONENTS_DNS_PREFETCH_RENDERER_PRESCIENT_NETWORKING_DISPATCHER_H_

#include "base/macros.h"
#include "components/dns_prefetch/renderer/renderer_net_predictor.h"
#include "third_party/WebKit/public/platform/WebPrescientNetworking.h"

namespace dns_prefetch {

class PrescientNetworkingDispatcher : public blink::WebPrescientNetworking {
 public:
  PrescientNetworkingDispatcher();
  ~PrescientNetworkingDispatcher() override;

  void prefetchDNS(const blink::WebString& hostname) override;

 private:
  dns_prefetch::RendererNetPredictor net_predictor_;

  DISALLOW_COPY_AND_ASSIGN(PrescientNetworkingDispatcher);
};

}   // namespace dns_prefetch

#endif  // COMPONENTS_DNS_PREFETCH_RENDERER_PRESCIENT_NETWORKING_DISPATCHER_H_
