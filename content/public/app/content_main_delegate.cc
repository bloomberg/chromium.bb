// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/app/content_main_delegate.h"

#include "build/build_config.h"

#include "content/public/gpu/content_gpu_client.h"
#include "content/public/renderer/content_renderer_client.h"
#include "content/public/utility/content_utility_client.h"

#if !defined(CHROME_MULTIPLE_DLL_CHILD)
#include "content/public/browser/content_browser_client.h"
#endif

namespace content {

bool ContentMainDelegate::BasicStartupComplete(int* exit_code) {
  return false;
}

int ContentMainDelegate::RunProcess(
    const std::string& process_type,
    const content::MainFunctionParams& main_function_params) {
  return -1;
}

#if defined(OS_MACOSX)

bool ContentMainDelegate::ProcessRegistersWithSystemProcess(
    const std::string& process_type) {
  return false;
}

bool ContentMainDelegate::ShouldSendMachPort(const std::string& process_type) {
  return true;
}

bool ContentMainDelegate::DelaySandboxInitialization(
    const std::string& process_type) {
  return false;
}

#elif defined(OS_POSIX) && !defined(OS_ANDROID)

void ContentMainDelegate::ZygoteStarting(
    std::vector<std::unique_ptr<ZygoteForkDelegate>>* delegates) {}

#endif

bool ContentMainDelegate::ShouldEnableProfilerRecording() {
  return false;
}

service_manager::ProcessType ContentMainDelegate::OverrideProcessType() {
  return service_manager::ProcessType::kDefault;
}

std::unique_ptr<base::Value> ContentMainDelegate::CreateServiceCatalog() {
  return nullptr;
}

void ContentMainDelegate::AdjustServiceProcessCommandLine(
    const service_manager::Identity& identity,
    base::CommandLine* command_line) {}

bool ContentMainDelegate::ShouldTerminateServiceManagerOnInstanceQuit(
    const service_manager::Identity& identity,
    int* exit_code) {
  return false;
}

void ContentMainDelegate::OnServiceManagerInitialized(
    const base::Closure& quit_closure,
    service_manager::BackgroundServiceManager* service_manager) {}

std::unique_ptr<service_manager::Service>
ContentMainDelegate::CreateEmbeddedService(const std::string& service_name) {
  return nullptr;
}

ContentBrowserClient* ContentMainDelegate::CreateContentBrowserClient() {
#if defined(CHROME_MULTIPLE_DLL_CHILD)
  return NULL;
#else
  return new ContentBrowserClient();
#endif
}

ContentGpuClient* ContentMainDelegate::CreateContentGpuClient() {
#if defined(CHROME_MULTIPLE_DLL_BROWSER)
  return NULL;
#else
  return new ContentGpuClient();
#endif
}

ContentRendererClient* ContentMainDelegate::CreateContentRendererClient() {
#if defined(CHROME_MULTIPLE_DLL_BROWSER)
  return NULL;
#else
  return new ContentRendererClient();
#endif
}

ContentUtilityClient* ContentMainDelegate::CreateContentUtilityClient() {
#if defined(CHROME_MULTIPLE_DLL_BROWSER)
  return NULL;
#else
  return new ContentUtilityClient();
#endif
}

}  // namespace content
