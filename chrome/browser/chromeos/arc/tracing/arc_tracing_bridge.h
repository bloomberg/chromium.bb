// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_TRACING_ARC_TRACING_BRIDGE_H_
#define CHROME_BROWSER_CHROMEOS_ARC_TRACING_ARC_TRACING_BRIDGE_H_

#include <string>
#include <vector>

#include "base/files/scoped_file.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/trace_event/trace_event.h"
#include "components/arc/common/tracing.mojom.h"
#include "components/arc/connection_observer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/arc_tracing_agent.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace arc {

class ArcBridgeService;

// This class provides the interface to trigger tracing in the container.
class ArcTracingBridge : public KeyedService,
                         public content::ArcTracingAgent::Delegate,
                         public ConnectionObserver<mojom::TracingInstance> {
 public:
  // Returns singleton instance for the given BrowserContext,
  // or nullptr if the browser |context| is not allowed to use ARC.
  static ArcTracingBridge* GetForBrowserContext(
      content::BrowserContext* context);

  ArcTracingBridge(content::BrowserContext* context,
                   ArcBridgeService* bridge_service);
  ~ArcTracingBridge() override;

  // ConnectionObserver<mojom::TracingInstance> overrides:
  void OnConnectionReady() override;

  // content::ArcTracingAgent::Delegate overrides:
  bool StartTracing(const base::trace_event::TraceConfig& trace_config,
                    base::ScopedFD write_fd,
                    const StartTracingCallback& callback) override;
  void StopTracing(const StopTracingCallback& callback) override;

 private:
  struct Category;

  // Callback for QueryAvailableCategories.
  void OnCategoriesReady(const std::vector<std::string>& categories);

  ArcBridgeService* const arc_bridge_service_;  // Owned by ArcServiceManager.

  // List of available categories.
  std::vector<Category> categories_;

  // NOTE: Weak pointers must be invalidated before all other member variables
  // so it must be the last member.
  base::WeakPtrFactory<ArcTracingBridge> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ArcTracingBridge);
};

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_TRACING_ARC_TRACING_BRIDGE_H_
