// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_UTILITY_UTILITY_THREAD_IMPL_H_
#define CONTENT_UTILITY_UTILITY_THREAD_IMPL_H_

#include <memory>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "content/child/child_thread_impl.h"
#include "content/common/content_export.h"
#include "content/public/utility/utility_thread.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/service_manager/public/interfaces/service_factory.mojom.h"

namespace service_manager {
struct BindSourceInfo;
}

namespace content {
class UtilityBlinkPlatformImpl;
class UtilityServiceFactory;

#if defined(COMPILER_MSVC)
// See explanation for other RenderViewHostImpl which is the same issue.
#pragma warning(push)
#pragma warning(disable: 4250)
#endif

// This class represents the background thread where the utility task runs.
class UtilityThreadImpl : public UtilityThread,
                          public ChildThreadImpl {
 public:
  UtilityThreadImpl();
  // Constructor that's used when running in single process mode.
  explicit UtilityThreadImpl(const InProcessChildThreadParams& params);
  ~UtilityThreadImpl() override;
  void Shutdown() override;

  void ReleaseProcessIfNeeded() override;
  void EnsureBlinkInitialized() override;

 private:
  void Init();

  // ChildThread implementation.
  bool OnControlMessageReceived(const IPC::Message& msg) override;

  // IPC message handlers.
  void OnBatchModeStarted();
  void OnBatchModeFinished();

  void BindServiceFactoryRequest(
      const service_manager::BindSourceInfo& source_info,
      service_manager::mojom::ServiceFactoryRequest request);

  // True when we're running in batch mode.
  bool batch_mode_;

  std::unique_ptr<UtilityBlinkPlatformImpl> blink_platform_impl_;

  // service_manager::mojom::ServiceFactory for service_manager::Service
  // hosting.
  std::unique_ptr<UtilityServiceFactory> service_factory_;

  // Bindings to the service_manager::mojom::ServiceFactory impl.
  mojo::BindingSet<service_manager::mojom::ServiceFactory>
      service_factory_bindings_;

  DISALLOW_COPY_AND_ASSIGN(UtilityThreadImpl);
};

#if defined(COMPILER_MSVC)
#pragma warning(pop)
#endif

}  // namespace content

#endif  // CONTENT_UTILITY_UTILITY_THREAD_IMPL_H_
