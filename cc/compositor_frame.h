// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_COMPOSITOR_FRAME_H_
#define CC_COMPOSITOR_FRAME_H_

#include "cc/cc_export.h"
#include "cc/render_pass.h"
#include "cc/scoped_ptr_vector.h"
#include "cc/transferable_resource.h"
#include "ui/gfx/size.h"

namespace cc {

class CC_EXPORT CompositorFrame {
 public:
  CompositorFrame();
  ~CompositorFrame();

  gfx::Size size;
  TransferableResourceList resource_list;
  ScopedPtrVector<RenderPass> render_pass_list;
};

}  // namespace cc

#endif  // CC_COMPOSITOR_FRAME_H_
