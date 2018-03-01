// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/protocol/memory_handler.h"

#include "base/memory/memory_pressure_listener.h"
#include "base/sampling_heap_profiler/sampling_heap_profiler.h"
#include "base/strings/stringprintf.h"
#include "content/public/common/content_features.h"

namespace content {
namespace protocol {

MemoryHandler::MemoryHandler()
    : DevToolsDomainHandler(Memory::Metainfo::domainName) {
}

MemoryHandler::~MemoryHandler() {}

void MemoryHandler::Wire(UberDispatcher* dispatcher) {
  Memory::Dispatcher::wire(dispatcher, this);
}

Response MemoryHandler::GetBrowserSamplingProfile(
    std::unique_ptr<Memory::SamplingProfile>* out_profile) {
  std::unique_ptr<Array<Memory::SamplingProfileNode>> samples =
      Array<Memory::SamplingProfileNode>::create();
  std::vector<base::SamplingHeapProfiler::Sample> raw_samples =
      base::SamplingHeapProfiler::GetInstance()->GetSamples(0);

  for (auto& sample : raw_samples) {
    std::unique_ptr<Array<String>> stack = Array<String>::create();
    for (auto* frame : sample.stack)
      stack->addItem(base::StringPrintf("%p", frame));
    samples->addItem(Memory::SamplingProfileNode::Create()
                         .SetSize(sample.size)
                         .SetTotal(sample.total)
                         .SetStack(std::move(stack))
                         .Build());
  }

  *out_profile =
      Memory::SamplingProfile::Create().SetSamples(std::move(samples)).Build();
  return Response::OK();
}

Response MemoryHandler::SetPressureNotificationsSuppressed(
    bool suppressed) {
  if (base::FeatureList::IsEnabled(features::kMemoryCoordinator)) {
    return Response::Error(
        "Cannot enable/disable notifications when memory coordinator is "
        "enabled");
  }

  base::MemoryPressureListener::SetNotificationsSuppressed(suppressed);
  return Response::OK();
}

Response MemoryHandler::SimulatePressureNotification(
    const std::string& level) {
  base::MemoryPressureListener::MemoryPressureLevel parsed_level;
  if (level == protocol::Memory::PressureLevelEnum::Moderate) {
    parsed_level = base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE;
  } else if (level == protocol::Memory::PressureLevelEnum::Critical) {
    parsed_level = base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL;
  } else {
    return Response::InvalidParams(base::StringPrintf(
        "Invalid memory pressure level '%s'", level.c_str()));
  }

  // Simulate memory pressure notification in the browser process.
  base::MemoryPressureListener::SimulatePressureNotification(parsed_level);
  return Response::OK();
}

}  // namespace protocol
}  // namespace content
