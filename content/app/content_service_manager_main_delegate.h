// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_APP_CONTENT_SERVICE_MANAGER_MAIN_DELEGATE_H_
#define CONTENT_APP_CONTENT_SERVICE_MANAGER_MAIN_DELEGATE_H_

#include <memory>

#include "base/macros.h"
#include "build/build_config.h"
#include "content/public/app/content_main.h"
#include "services/service_manager/embedder/main_delegate.h"

namespace content {

class ContentMainRunner;

class ContentServiceManagerMainDelegate : public service_manager::MainDelegate {
 public:
  ContentServiceManagerMainDelegate(const ContentMainParams& params);
  ~ContentServiceManagerMainDelegate() override;

  // service_manager::MainDelegate:
  int Initialize(const InitializeParams& params) override;
  int Run() override;
  void ShutDown() override;

 private:
  ContentMainParams content_main_params_;
  std::unique_ptr<ContentMainRunner> content_main_runner_;

#if defined(OS_ANDROID)
  bool initialized_ = false;
#endif

  DISALLOW_COPY_AND_ASSIGN(ContentServiceManagerMainDelegate);
};

}  // namespace content

#endif  // CONTENT_APP_CONTENT_SERVICE_MANAGER_MAIN_DELEGATE_H_
