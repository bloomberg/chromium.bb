// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/app/content_service_manager_main_delegate.h"

#include "content/public/app/content_main_runner.h"

namespace content {

ContentServiceManagerMainDelegate::ContentServiceManagerMainDelegate(
    const ContentMainParams& params)
    : content_main_params_(params),
      content_main_runner_(ContentMainRunner::Create()) {}

ContentServiceManagerMainDelegate::~ContentServiceManagerMainDelegate() =
    default;

int ContentServiceManagerMainDelegate::Initialize(
    const InitializeParams& params) {
#if defined(OS_ANDROID)
  // May be called twice on Android due to the way browser startup requests are
  // dispatched by the system.
  if (initialized_)
    return -1;
#endif

#if defined(OS_MACOSX)
  content_main_params_.autorelease_pool = params.autorelease_pool;
#endif

  return content_main_runner_->Initialize(content_main_params_);
}

int ContentServiceManagerMainDelegate::Run() {
  return content_main_runner_->Run();
}

void ContentServiceManagerMainDelegate::ShutDown() {
#if !defined(OS_ANDROID)
  content_main_runner_->Shutdown();
#endif
}

}  // namespace content
