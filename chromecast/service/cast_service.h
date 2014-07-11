// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_SERVICE_CAST_SERVICE_H_
#define CHROMECAST_SERVICE_CAST_SERVICE_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"

namespace aura {
class WindowTreeHost;
}

namespace base {
class ThreadChecker;
}

namespace content{
class BrowserContext;
class WebContents;
}

namespace chromecast {

class CastService {
 public:
  explicit CastService(content::BrowserContext* browser_context);
  virtual ~CastService();

  // Start/stop the cast service.
  void Start();
  void Stop();

 private:
  // Platform specific initialization if any.
  static void PlatformInitialize();

  void Initialize();

  content::BrowserContext* const browser_context_;
  scoped_ptr<aura::WindowTreeHost> window_tree_host_;
  scoped_ptr<content::WebContents> web_contents_;
  bool stopped_;

  const scoped_ptr<base::ThreadChecker> thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(CastService);
};

}  // namespace chromecast

#endif  // CHROMECAST_SERVICE_CAST_SERVICE_H_
