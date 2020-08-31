// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/app/content_main_delegate.h"

#include "base/check.h"
#include "build/build_config.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"
#include "content/public/gpu/content_gpu_client.h"
#include "content/public/renderer/content_renderer_client.h"
#include "content/public/utility/content_utility_client.h"

namespace content {

bool ContentMainDelegate::BasicStartupComplete(int* exit_code) {
  return false;
}

int ContentMainDelegate::RunProcess(
    const std::string& process_type,
    const content::MainFunctionParams& main_function_params) {
  return -1;
}

#if defined(OS_LINUX)

void ContentMainDelegate::ZygoteStarting(
    std::vector<std::unique_ptr<service_manager::ZygoteForkDelegate>>*
        delegates) {}

#endif  // defined(OS_LINUX)

int ContentMainDelegate::TerminateForFatalInitializationError() {
  CHECK(false);
  return 0;
}

service_manager::ProcessType ContentMainDelegate::OverrideProcessType() {
  return service_manager::ProcessType::kDefault;
}

void ContentMainDelegate::AdjustServiceProcessCommandLine(
    const service_manager::Identity& identity,
    base::CommandLine* command_line) {}

void ContentMainDelegate::OnServiceManagerInitialized(
    base::OnceClosure quit_closure,
    service_manager::BackgroundServiceManager* service_manager) {}

bool ContentMainDelegate::ShouldCreateFeatureList() {
  return true;
}

ContentClient* ContentMainDelegate::CreateContentClient() {
  return new ContentClient();
}

ContentBrowserClient* ContentMainDelegate::CreateContentBrowserClient() {
  return new ContentBrowserClient();
}

ContentGpuClient* ContentMainDelegate::CreateContentGpuClient() {
  return new ContentGpuClient();
}

ContentRendererClient* ContentMainDelegate::CreateContentRendererClient() {
  return new ContentRendererClient();
}

ContentUtilityClient* ContentMainDelegate::CreateContentUtilityClient() {
  return new ContentUtilityClient();
}

}  // namespace content
