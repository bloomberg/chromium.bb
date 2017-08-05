// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_HIT_TEST_HIT_TEST_AGGREGATOR_DELEGATE_H_
#define COMPONENTS_VIZ_SERVICE_HIT_TEST_HIT_TEST_AGGREGATOR_DELEGATE_H_

namespace viz {
// Used by HitTestAggregator to talk to GpuRootCompositorFrameSink.
class HitTestAggregatorDelegate {
 public:
  // Called if any of the buffer that stores the aggregated hit-test data is
  // updated (e.g. destroyed, reallocated etc.). |active_handle| and
  // |idle_handle| both must be valid.
  virtual void OnAggregatedHitTestRegionListUpdated(
      mojo::ScopedSharedBufferHandle active_handle,
      uint32_t active_handle_size,
      mojo::ScopedSharedBufferHandle idle_handle,
      uint32_t idle_handle_size) = 0;

  virtual void SwitchActiveAggregatedHitTestRegionList(
      uint8_t active_handle_index) = 0;

 protected:
  // The dtor is protected so that HitTestAggregator does not take ownership.
  virtual ~HitTestAggregatorDelegate() {}
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_HIT_TEST_HIT_TEST_AGGREGATOR_DELEGATE_H_
