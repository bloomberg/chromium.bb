// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_COMPOSITOR_FRAME_PRODUCER_H_
#define ANDROID_WEBVIEW_BROWSER_COMPOSITOR_FRAME_PRODUCER_H_

#include <vector>

#include "android_webview/browser/compositor_id.h"
#include "base/memory/weak_ptr.h"
#include "components/viz/common/resources/returned_resource.h"

namespace android_webview {

class CompositorFrameConsumer;

class CompositorFrameProducer {
 public:
  virtual base::WeakPtr<CompositorFrameProducer> GetWeakPtr() = 0;
  virtual void ReturnUsedResources(
      const std::vector<viz::ReturnedResource>& resources,
      const CompositorID& compositor_id,
      uint32_t layer_tree_frame_sink_id) = 0;
  virtual void OnParentDrawConstraintsUpdated(
      CompositorFrameConsumer* compositor_frame_consumer) = 0;
  virtual void RemoveCompositorFrameConsumer(
      CompositorFrameConsumer* consumer) = 0;

 protected:
  virtual ~CompositorFrameProducer() {}
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_COMPOSITOR_FRAME_PRODUCER_H_
