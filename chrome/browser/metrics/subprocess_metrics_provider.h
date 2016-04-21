// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_SUBPROCESS_METRICS_PROVIDER_H_
#define CHROME_BROWSER_METRICS_SUBPROCESS_METRICS_PROVIDER_H_

#include <memory>
#include <set>

#include "base/gtest_prod_util.h"
#include "base/id_map.h"
#include "base/scoped_observer.h"
#include "base/threading/thread_checker.h"
#include "components/metrics/metrics_provider.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/render_process_host_observer.h"

namespace base {
class PersistentHistogramAllocator;
class SharedPersistentMemoryAllocator;
}

// SubprocessMetricsProvider gathers and logs histograms stored in shared
// memory segments between processes.
class SubprocessMetricsProvider : public metrics::MetricsProvider,
                                  public content::NotificationObserver,
                                  public content::RenderProcessHostObserver {
 public:
  SubprocessMetricsProvider();
  ~SubprocessMetricsProvider() override;

 private:
  friend class SubprocessMetricsProviderTest;

  // Indicates subprocess to be monitored with unique id for later reference.
  // Metrics reporting will read histograms from it and upload them to UMA.
  void RegisterSubprocessAllocator(
      int id,
      std::unique_ptr<base::PersistentHistogramAllocator> allocator);

  // Indicates that a subprocess has exited and is thus finished with the
  // allocator it was using.
  void DeregisterSubprocessAllocator(int id);

  // Report all histograms of a given allocator to the snapshot-manager.
  void RecordHistogramSnapshotsFromAllocator(
      base::HistogramSnapshotManager* snapshot_manager,
      int id,
      base::PersistentHistogramAllocator* allocator);

  // metrics::MetricsProvider:
  void OnDidCreateMetricsLog() override;
  void OnRecordingEnabled() override;
  void OnRecordingDisabled() override;
  void RecordHistogramSnapshots(
      base::HistogramSnapshotManager* snapshot_manager) override;

  // content::NotificationObserver:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  // content::RenderProcessHostObserver:
  void RenderProcessReady(content::RenderProcessHost* host) override;
  void RenderProcessExited(content::RenderProcessHost* host,
                           base::TerminationStatus status,
                           int exit_code) override;
  void RenderProcessHostDestroyed(content::RenderProcessHost* host) override;

  base::ThreadChecker thread_checker_;

  // Object for registing notification requests.
  content::NotificationRegistrar registrar_;

  // All of the shared-persistent-allocators for known sub-processes.
  using AllocatorByIdMap =
      IDMap<base::PersistentHistogramAllocator, IDMapOwnPointer, int>;
  AllocatorByIdMap allocators_by_id_;

  // Allocators that are no longer attached to a subprocess, to be released
  // once the last data contained therein has been reported.
  std::vector<std::unique_ptr<base::PersistentHistogramAllocator>>
      allocators_for_exited_processes_;
  std::vector<std::unique_ptr<base::PersistentHistogramAllocator>>
      allocators_to_release_;

  // Track all observed render processes to un-observe them on exit.
  ScopedObserver<content::RenderProcessHost, SubprocessMetricsProvider>
      scoped_observer_;

  // Flag indicating if metrics recording is enabled. Allocators will not
  // live past the death of the subprocess if it is not.
  bool metrics_recording_enabled_ = false;

  DISALLOW_COPY_AND_ASSIGN(SubprocessMetricsProvider);
};

#endif  // CHROME_BROWSER_METRICS_SUBPROCESS_METRICS_PROVIDER_H_
