// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_SERVICE_CAST_SERVICE_SIMPLE_H_
#define CHROMECAST_BROWSER_SERVICE_CAST_SERVICE_SIMPLE_H_

#include "base/memory/scoped_ptr.h"
#include "chromecast/browser/service/cast_service.h"

namespace content {
class WebContents;
}

namespace chromecast {
class CastContentWindow;

class CastServiceSimple : public CastService {
 public:
  CastServiceSimple(content::BrowserContext* browser_context,
                    const OptInStatsChangedCallback& opt_in_stats_callback);
  virtual ~CastServiceSimple();

 protected:
  // CastService implementation:
  virtual void Initialize() override;
  virtual void StartInternal() override;
  virtual void StopInternal() override;

 private:
  scoped_ptr<CastContentWindow> window_;
  scoped_ptr<content::WebContents> web_contents_;

  DISALLOW_COPY_AND_ASSIGN(CastServiceSimple);
};

}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_SERVICE_CAST_SERVICE_SIMPLE_H_
