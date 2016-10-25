// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/public/app/web_main.h"
#include "ios/web/public/app/web_main_runner.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

WebMain::WebMain(const WebMainParams& params) {
  web_main_runner_.reset(WebMainRunner::Create());
  web_main_runner_->Initialize(params);
}

WebMain::~WebMain() {
  web_main_runner_->ShutDown();
}

}  // namespace web
