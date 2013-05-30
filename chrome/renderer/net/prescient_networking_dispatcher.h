// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_NET_PRESCIENT_NETWORKING_DISPATCHER_H_
#define CHROME_RENDERER_NET_PRESCIENT_NETWORKING_DISPATCHER_H_

#include "base/compiler_specific.h"
#include "third_party/WebKit/public/platform/WebPrescientNetworking.h"

class PrescientNetworkingDispatcher : public WebKit::WebPrescientNetworking {
 public:
  virtual ~PrescientNetworkingDispatcher();

  virtual void preconnect(const WebKit::WebURL& url,
                          WebKit::WebPreconnectMotivation motivation) OVERRIDE;
};

#endif  // CHROME_RENDERER_NET_PRESCIENT_NETWORKING_DISPATCHER_H_
