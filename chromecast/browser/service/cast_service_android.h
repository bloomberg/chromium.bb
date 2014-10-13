// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_SERVICE_CAST_SERVICE_ANDROID_H_
#define CHROMECAST_BROWSER_SERVICE_CAST_SERVICE_ANDROID_H_

#include "base/macros.h"
#include "chromecast/browser/service/cast_service.h"

namespace chromecast {

class CastServiceAndroid : public CastService {
 public:
  CastServiceAndroid(content::BrowserContext* browser_context,
                     const OptInStatsChangedCallback& opt_in_stats_callback);
  virtual ~CastServiceAndroid();

 protected:
  // CastService implementation.
  virtual void Initialize() override;
  virtual void StartInternal() override;
  virtual void StopInternal() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(CastServiceAndroid);
};

}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_SERVICE_CAST_SERVICE_ANDROID_H_
