// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/engine/browser/blimp_browser_main_parts.h"

#include "blimp/engine/browser/blimp_browser_context.h"
#include "blimp/engine/browser/blimp_engine_session.h"
#include "blimp/net/blimp_client_session.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/main_function_params.h"
#include "net/base/net_module.h"
#include "net/log/net_log.h"

namespace blimp {
namespace engine {

BlimpBrowserMainParts::BlimpBrowserMainParts(
    const content::MainFunctionParams& parameters) {}

BlimpBrowserMainParts::~BlimpBrowserMainParts() {}

void BlimpBrowserMainParts::PreMainMessageLoopRun() {
  net_log_.reset(new net::NetLog());
  scoped_ptr<BlimpBrowserContext> browser_context(
      new BlimpBrowserContext(false, net_log_.get()));
  engine_session_.reset(new BlimpEngineSession(browser_context.Pass()));
  engine_session_->Initialize();
  // TODO(haibinlu): remove this after a real client session can be attached.
  scoped_ptr<BlimpClientSession> startupSession(new BlimpClientSession);
  engine_session_->AttachClientSession(startupSession.Pass());
}

void BlimpBrowserMainParts::PostMainMessageLoopRun() {
  engine_session_.reset();
}

BlimpBrowserContext* BlimpBrowserMainParts::GetBrowserContext() {
  return engine_session_->browser_context();
}

}  // namespace engine
}  // namespace blimp
