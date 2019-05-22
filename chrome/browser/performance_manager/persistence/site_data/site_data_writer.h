// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_PERSISTENCE_SITE_DATA_SITE_DATA_WRITER_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_PERSISTENCE_SITE_DATA_SITE_DATA_WRITER_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/performance_manager/persistence/site_data/site_data_impl.h"

namespace performance_manager {

// This writer is initially in an unloaded state, a |NotifySiteLoaded| event
// should be sent if/when the tab using it gets loaded.
class SiteDataWriter {
 public:
  ~SiteDataWriter();

  // Records tab load/unload events.
  void NotifySiteLoaded();
  void NotifySiteUnloaded();

  // Records visibility change events.
  void NotifySiteVisibilityChanged(
      performance_manager::TabVisibility visibility);

  // Records feature usage.
  void NotifyUpdatesFaviconInBackground();
  void NotifyUpdatesTitleInBackground();
  void NotifyUsesAudioInBackground();
  void NotifyUsesNotificationsInBackground();

  // Records performance measurements.
  void NotifyLoadTimePerformanceMeasurement(
      base::TimeDelta load_duration,
      base::TimeDelta cpu_usage_estimate,
      uint64_t private_footprint_kb_estimate);

  internal::SiteDataImpl* impl_for_testing() const { return impl_.get(); }

 private:
  friend class SiteDataWriterTest;
  friend class SiteDataStore;

  // Private constructor, these objects are meant to be created by a site data
  // store.
  SiteDataWriter(scoped_refptr<internal::SiteDataImpl> impl,
                 performance_manager::TabVisibility tab_visibility);

  // The SiteDataImpl object we delegate to.
  const scoped_refptr<internal::SiteDataImpl> impl_;

  // The visibility of the tab using this writer.
  performance_manager::TabVisibility tab_visibility_;

  // Indicates if the tab using this writer is loaded.
  bool is_loaded_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(SiteDataWriter);
};

}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_PERSISTENCE_SITE_DATA_SITE_DATA_WRITER_H_
