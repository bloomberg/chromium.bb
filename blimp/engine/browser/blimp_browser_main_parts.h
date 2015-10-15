// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_ENGINE_BROWSER_BLIMP_BROWSER_MAIN_PARTS_H_
#define BLIMP_ENGINE_BROWSER_BLIMP_BROWSER_MAIN_PARTS_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "content/public/browser/browser_main_parts.h"

namespace net {
class NetLog;
}

namespace content {
struct MainFunctionParams;
}

namespace blimp {
namespace engine {

class BlimpBrowserContext;
class BlimpEngineSession;

class BlimpBrowserMainParts : public content::BrowserMainParts {
 public:
  explicit BlimpBrowserMainParts(const content::MainFunctionParams& parameters);
  ~BlimpBrowserMainParts() override;

  // content::BrowserMainParts implementation.
  void PreMainMessageLoopRun() override;
  void PostMainMessageLoopRun() override;

  BlimpBrowserContext* GetBrowserContext();

 private:
  scoped_ptr<net::NetLog> net_log_;
  scoped_ptr<BlimpEngineSession> engine_session_;

  DISALLOW_COPY_AND_ASSIGN(BlimpBrowserMainParts);
};

}  // namespace engine
}  // namespace blimp

#endif  // BLIMP_ENGINE_BROWSER_BLIMP_BROWSER_MAIN_PARTS_H_
