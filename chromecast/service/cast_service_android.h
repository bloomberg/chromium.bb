// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_SERVICE_CAST_SERVICE_ANDROID_H_
#define CHROMECAST_SERVICE_CAST_SERVICE_ANDROID_H_

#include "base/macros.h"
#include "chromecast/service/cast_service.h"

namespace chromecast {

class CastServiceAndroid : public CastService {
 public:
  explicit CastServiceAndroid(content::BrowserContext* browser_context);
  virtual ~CastServiceAndroid();

 protected:
  // CastService implementation.
  virtual void Initialize() OVERRIDE;
  virtual void StartInternal() OVERRIDE;
  virtual void StopInternal() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(CastServiceAndroid);
};

}  // namespace chromecast

#endif  // CHROMECAST_SERVICE_CAST_SERVICE_ANDROID_H_
