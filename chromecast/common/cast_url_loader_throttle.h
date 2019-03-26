// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_COMMON_CAST_URL_LOADER_THROTTLE_H_
#define CHROMECAST_COMMON_CAST_URL_LOADER_THROTTLE_H_

#include "base/macros.h"
#include "content/public/common/url_loader_throttle.h"

namespace chromecast {

class CastURLLoaderThrottle : public content::URLLoaderThrottle {
 public:
  CastURLLoaderThrottle();
  ~CastURLLoaderThrottle() override;

  // content::URLLoaderThrottle implementation:
  void DetachFromCurrentSequence() override;
  void WillStartRequest(network::ResourceRequest* request,
                        bool* defer) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(CastURLLoaderThrottle);
};

}  // namespace chromecast

#endif  // CHROMECAST_COMMON_CAST_URL_LOADER_THROTTLE_H_
