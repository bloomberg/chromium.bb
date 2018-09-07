// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_GPU_INTERFACE_PROVIDER_H_
#define CONTENT_BROWSER_GPU_INTERFACE_PROVIDER_H_

#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "content/common/content_export.h"
#include "services/ws/public/cpp/host/gpu_interface_provider.h"

namespace content {

// An implementation of GpuInterfaceProvider that forwards to the Gpu
// implementation in content.
class CONTENT_EXPORT GpuInterfaceProvider : public ws::GpuInterfaceProvider {
 public:
  GpuInterfaceProvider();
  ~GpuInterfaceProvider() override;

  // ws::GpuInterfaceProvider:
  void RegisterGpuInterfaces(
      service_manager::BinderRegistry* registry) override;
#if defined(USE_OZONE)
  void RegisterOzoneGpuInterfaces(
      service_manager::BinderRegistry* registry) override;
#endif

 private:
  class InterfaceBinderImpl;

  scoped_refptr<InterfaceBinderImpl> interface_binder_impl_;

  DISALLOW_COPY_AND_ASSIGN(GpuInterfaceProvider);
};

}  // namespace content

#endif  // CONTENT_BROWSER_GPU_INTERFACE_PROVIDER_H_
