// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/heap_profiling/public/cpp/manifest.h"

#include "base/no_destructor.h"
#include "components/services/heap_profiling/public/mojom/constants.mojom.h"
#include "components/services/heap_profiling/public/mojom/heap_profiling_service.mojom.h"
#include "services/resource_coordinator/public/mojom/memory_instrumentation/memory_instrumentation.mojom.h"
#include "services/resource_coordinator/public/mojom/service_constants.mojom.h"
#include "services/service_manager/public/cpp/manifest_builder.h"

namespace heap_profiling {

const service_manager::Manifest& GetManifest() {
  static base::NoDestructor<service_manager::Manifest> manifest{
      service_manager::ManifestBuilder()
          .WithServiceName(mojom::kServiceName)
          .WithDisplayName("Heap Profiling Service")
          .WithOptions(service_manager::ManifestOptionsBuilder()
                           .WithSandboxType("profiling")
                           .WithInstanceSharingPolicy(
                               service_manager::Manifest::
                                   InstanceSharingPolicy::kSharedAcrossGroups)
                           .Build())
          .ExposeCapability("profiling",
                            service_manager::Manifest::InterfaceList<
                                mojom::ProfilingService>())
          .ExposeCapability("heap_profiler",
                            service_manager::Manifest::InterfaceList<
                                memory_instrumentation::mojom::HeapProfiler>())
          .RequireCapability(resource_coordinator::mojom::kServiceName,
                             "heap_profiler_helper")
          .Build()};
  return *manifest;
}

}  // namespace heap_profiling
