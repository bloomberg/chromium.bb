// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_TRACING_ARC_TRACING_BRIDGE_H_
#define CHROME_BROWSER_CHROMEOS_ARC_TRACING_ARC_TRACING_BRIDGE_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/trace_event/trace_event.h"
#include "components/arc/arc_service.h"
#include "components/arc/common/tracing.mojom.h"
#include "components/arc/instance_holder.h"
#include "content/browser/tracing/arc_tracing_agent.h"

namespace arc {

class ArcBridgeService;

// This class provides the interface to trigger tracing in the container.
class ArcTracingBridge
    : public ArcService,
      public content::ArcTracingAgent::Delegate,
      public InstanceHolder<mojom::TracingInstance>::Observer {
 public:
  explicit ArcTracingBridge(ArcBridgeService* bridge_service);
  ~ArcTracingBridge() override;

  // InstanceHolder<mojom::TracingInstance>::Observer overrides:
  void OnInstanceReady() override;

  // content::ArcTracingAgent::Delegate overrides:
  void StartTracing(const base::trace_event::TraceConfig& trace_config,
                    const StartTracingCallback& callback) override;
  void StopTracing(const StopTracingCallback& callback) override;

 private:
  struct Category;

  // Callback for QueryAvailableCategories.
  void OnCategoriesReady(const std::vector<std::string>& categories);

  // List of available categories.
  std::vector<Category> categories_;

  // NOTE: Weak pointers must be invalidated before all other member variables
  // so it must be the last member.
  base::WeakPtrFactory<ArcTracingBridge> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ArcTracingBridge);
};

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_TRACING_ARC_TRACING_BRIDGE_H_
