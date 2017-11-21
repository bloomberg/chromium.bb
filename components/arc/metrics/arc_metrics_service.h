// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_METRICS_ARC_METRICS_SERVICE_H_
#define COMPONENTS_ARC_METRICS_ARC_METRICS_SERVICE_H_

#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/threading/thread_checker.h"
#include "base/timer/timer.h"
#include "components/arc/common/metrics.mojom.h"
#include "components/arc/common/process.mojom.h"
#include "components/arc/connection_observer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace arc {

class ArcBridgeService;

// Collects information from other ArcServices and send UMA metrics.
class ArcMetricsService : public KeyedService,
                          public ConnectionObserver<mojom::MetricsInstance>,
                          public mojom::MetricsHost {
 public:
  // Returns singleton instance for the given BrowserContext,
  // or nullptr if the browser |context| is not allowed to use ARC.
  static ArcMetricsService* GetForBrowserContext(
      content::BrowserContext* context);

  ArcMetricsService(content::BrowserContext* context,
                    ArcBridgeService* bridge_service);
  ~ArcMetricsService() override;

  // ConnectionObserver<mojom::MetricsInstance> overrides.
  void OnConnectionReady() override;
  void OnConnectionClosed() override;

  // Implementations for ConnectionObserver<mojom::ProcessInstance>.
  void OnProcessConnectionReady();
  void OnProcessConnectionClosed();

  // MetricsHost overrides.
  void ReportBootProgress(std::vector<mojom::BootProgressEventPtr> events,
                          mojom::BootType boot_type) override;

 private:
  // Adapter to be able to also observe ProcessInstance events.
  class ProcessObserver : public ConnectionObserver<mojom::ProcessInstance> {
   public:
    explicit ProcessObserver(ArcMetricsService* arc_metrics_service);
    ~ProcessObserver() override;

   private:
    // ConnectionObserver<mojom::ProcessInstance> overrides.
    void OnConnectionReady() override;
    void OnConnectionClosed() override;

    ArcMetricsService* arc_metrics_service_;

    DISALLOW_COPY_AND_ASSIGN(ProcessObserver);
  };

  void RequestProcessList();
  void ParseProcessList(std::vector<mojom::RunningAppProcessInfoPtr> processes);

  // DBus callbacks.
  void OnArcStartTimeRetrieved(base::Optional<base::TimeTicks> arc_start_time);

  THREAD_CHECKER(thread_checker_);

  ArcBridgeService* const arc_bridge_service_;  // Owned by ArcServiceManager.

  mojo::Binding<mojom::MetricsHost> binding_;

  ProcessObserver process_observer_;
  base::RepeatingTimer timer_;

  base::TimeTicks arc_start_time_;

  // Always keep this the last member of this class to make sure it's the
  // first thing to be destructed.
  base::WeakPtrFactory<ArcMetricsService> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ArcMetricsService);
};

}  // namespace arc

#endif  // COMPONENTS_ARC_METRICS_ARC_METRICS_SERVICE_H_
