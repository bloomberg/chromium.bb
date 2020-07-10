// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_APP_CONTENT_SERVICE_MANAGER_MAIN_DELEGATE_H_
#define CONTENT_APP_CONTENT_SERVICE_MANAGER_MAIN_DELEGATE_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/single_thread_task_runner.h"
#include "build/build_config.h"
#include "content/public/app/content_main.h"
#include "services/service_manager/embedder/main_delegate.h"

namespace content {

class ContentMainRunnerImpl;

class ContentServiceManagerMainDelegate : public service_manager::MainDelegate {
 public:
  explicit ContentServiceManagerMainDelegate(const ContentMainParams& params);
  ~ContentServiceManagerMainDelegate() override;

  // service_manager::MainDelegate:
  int Initialize(const InitializeParams& params) override;
  bool IsEmbedderSubprocess() override;
  int RunEmbedderProcess() override;
  void ShutDownEmbedderProcess() override;
  service_manager::ProcessType OverrideProcessType() override;
  void OverrideMojoConfiguration(mojo::core::Configuration* config) override;
  std::vector<service_manager::Manifest> GetServiceManifests() override;
  bool ShouldLaunchAsServiceProcess(
      const service_manager::Identity& identity) override;
  void AdjustServiceProcessCommandLine(
      const service_manager::Identity& identity,
      base::CommandLine* command_line) override;
  void OnServiceManagerInitialized(
      base::OnceClosure quit_closure,
      service_manager::BackgroundServiceManager* service_manager) override;
  std::unique_ptr<service_manager::Service> CreateEmbeddedService(
      const std::string& service_name) override;

  // Sets the flag whether to start the Service Manager without starting the
  // full browser.
  void SetStartServiceManagerOnly(bool start_service_manager_only);

 private:
  ContentMainParams content_main_params_;
  std::unique_ptr<ContentMainRunnerImpl> content_main_runner_;

#if defined(OS_ANDROID)
  bool initialized_ = false;
#endif

  // Indicates whether to start the Service Manager without starting the full
  // browser.
  bool start_service_manager_only_ = false;

  DISALLOW_COPY_AND_ASSIGN(ContentServiceManagerMainDelegate);
};

}  // namespace content

#endif  // CONTENT_APP_CONTENT_SERVICE_MANAGER_MAIN_DELEGATE_H_
