// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_BLIMP_COMPOSITOR_STATE_DESERIALIZER_CLIENT_H_
#define CC_BLIMP_COMPOSITOR_STATE_DESERIALIZER_CLIENT_H_

#include "base/macros.h"
#include "cc/base/cc_export.h"

namespace gfx {
class ScrollOffset;
}  // namespace gfx

namespace cc {

class CC_EXPORT CompositorStateDeserializerClient {
 public:
  virtual ~CompositorStateDeserializerClient() {}

  // Returns true if the given scroll offset update received from the engine
  // should be ignored.
  virtual bool ShouldRetainClientScroll(
      int engine_layer_id,
      const gfx::ScrollOffset& new_offset) = 0;

  // Returns true if the given page scale update from the engine should be
  // ignored.
  virtual bool ShouldRetainClientPageScale(float new_page_scale) = 0;
};

}  // namespace cc

#endif  // CC_BLIMP_COMPOSITOR_STATE_DESERIALIZER_CLIENT_H_
