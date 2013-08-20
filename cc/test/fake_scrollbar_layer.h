// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_FAKE_SCROLLBAR_LAYER_H_
#define CC_TEST_FAKE_SCROLLBAR_LAYER_H_

#include "base/memory/scoped_ptr.h"
#include "cc/layers/scrollbar_layer.h"
#include "cc/test/fake_scrollbar.h"

namespace base { template<typename T> class AutoReset; }

namespace cc {

class FakeScrollbarLayer : public ScrollbarLayer {
 public:
  static scoped_refptr<FakeScrollbarLayer> Create(bool paint_during_update,
                                                  bool has_thumb,
                                                  int scrolling_layer_id);
  int update_count() const { return update_count_; }
  void reset_update_count() { update_count_ = 0; }

  virtual bool Update(ResourceUpdateQueue* queue,
                      const OcclusionTracker* occlusion) OVERRIDE;

  virtual void PushPropertiesTo(LayerImpl* layer) OVERRIDE;

  scoped_ptr<base::AutoReset<bool> > IgnoreSetNeedsCommit();

  size_t push_properties_count() const { return push_properties_count_; }
  void reset_push_properties_count() { push_properties_count_ = 0; }

  // For unit tests
  UIResourceId track_resource_id() {
    return ScrollbarLayer::track_resource_id();
  }
  UIResourceId thumb_resource_id() {
    return ScrollbarLayer::thumb_resource_id();
  }
  FakeScrollbar* fake_scrollbar() {
    return fake_scrollbar_;
  }
  using ScrollbarLayer::UpdateThumbAndTrackGeometry;

 private:
  FakeScrollbarLayer(FakeScrollbar* fake_scrollbar,
                     int scrolling_layer_id);
  virtual ~FakeScrollbarLayer();

  int update_count_;
  size_t push_properties_count_;
  FakeScrollbar* fake_scrollbar_;
};

}  // namespace cc

#endif  // CC_TEST_FAKE_SCROLLBAR_LAYER_H_
