// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_NET_PRESCIENT_NETWORKING_DISPATCHER_H_
#define CHROME_RENDERER_NET_PRESCIENT_NETWORKING_DISPATCHER_H_

#include "base/compiler_specific.h"
#include "chrome/renderer/net/renderer_net_predictor.h"
#include "third_party/WebKit/public/platform/WebPrescientNetworking.h"

class PrescientNetworkingDispatcher : public blink::WebPrescientNetworking {
 public:
  PrescientNetworkingDispatcher();
  virtual ~PrescientNetworkingDispatcher();

  virtual void prefetchDNS(const blink::WebString& hostname) OVERRIDE;
 private:
  RendererNetPredictor net_predictor_;

  DISALLOW_COPY_AND_ASSIGN(PrescientNetworkingDispatcher);
};

#endif  // CHROME_RENDERER_NET_PRESCIENT_NETWORKING_DISPATCHER_H_
