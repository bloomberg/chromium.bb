// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_NET_PRESCIENT_NETWORKING_DISPATCHER_H_
#define CHROME_RENDERER_NET_PRESCIENT_NETWORKING_DISPATCHER_H_

#include "base/compiler_specific.h"
#include "chrome/renderer/net/renderer_net_predictor.h"
#include "third_party/WebKit/public/platform/WebPrescientNetworking.h"

class PrescientNetworkingDispatcher : public WebKit::WebPrescientNetworking {
 public:
  PrescientNetworkingDispatcher();
  virtual ~PrescientNetworkingDispatcher();

  virtual void prefetchDNS(const WebKit::WebString& hostname) OVERRIDE;

  virtual void preconnect(const WebKit::WebURL& url,
                          WebKit::WebPreconnectMotivation motivation) OVERRIDE;
 private:
  RendererNetPredictor net_predictor_;

  DISALLOW_COPY_AND_ASSIGN(PrescientNetworkingDispatcher);
};

#endif  // CHROME_RENDERER_NET_PRESCIENT_NETWORKING_DISPATCHER_H_
