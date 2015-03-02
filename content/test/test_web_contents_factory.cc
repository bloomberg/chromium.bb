// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/test_web_contents_factory.h"

#include "base/run_loop.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"

#if defined(USE_AURA)
#include "ui/aura/env.h"
#endif

namespace content {

TestWebContentsFactory::TestWebContentsFactory()
    : rvh_enabler_(new content::RenderViewHostTestEnabler()),
      tear_down_aura_(false) {
#if defined(USE_AURA)
  if (aura::Env::GetInstanceDontCreate() == nullptr) {
    aura::Env::CreateInstance(true);
    tear_down_aura_ = true;
  }
#endif
}

TestWebContentsFactory::~TestWebContentsFactory() {
  // We explicitly clear the vector to force destruction of any created web
  // contents so that we can properly handle their cleanup (running posted
  // tasks, etc).
  web_contents_.clear();
  // Let any posted tasks for web contents deletion run.
  base::RunLoop().RunUntilIdle();
  rvh_enabler_.reset();
  // Let any posted tasks for RenderProcess/ViewHost deletion run.
  base::RunLoop().RunUntilIdle();
#if defined(USE_AURA)
  if (tear_down_aura_)
    aura::Env::DeleteInstance();
#endif
}

WebContents* TestWebContentsFactory::CreateWebContents(
    BrowserContext* context) {
  web_contents_.push_back(
      WebContentsTester::CreateTestWebContents(context, nullptr));
  DCHECK(web_contents_.back());
  return web_contents_.back();
}

}  // namespace content
