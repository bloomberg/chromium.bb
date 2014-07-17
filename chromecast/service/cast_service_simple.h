// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_SERVICE_CAST_SERVICE_SIMPLE_H_
#define CHROMECAST_SERVICE_CAST_SERVICE_SIMPLE_H_

#include "base/memory/scoped_ptr.h"
#include "chromecast/service/cast_service.h"

namespace aura {
class WindowTreeHost;
}

namespace content {
class WebContents;
}

namespace chromecast {

class CastServiceSimple : public CastService {
 public:
  explicit CastServiceSimple(content::BrowserContext* browser_context);
  virtual ~CastServiceSimple();

 protected:
  // CastService implementation.
  virtual void Initialize() OVERRIDE;
  virtual void StartInternal() OVERRIDE;
  virtual void StopInternal() OVERRIDE;

 private:
  scoped_ptr<aura::WindowTreeHost> window_tree_host_;
  scoped_ptr<content::WebContents> web_contents_;

  DISALLOW_COPY_AND_ASSIGN(CastServiceSimple);
};

}  // namespace chromecast

#endif  // CHROMECAST_SERVICE_CAST_SERVICE_SIMPLE_H_
