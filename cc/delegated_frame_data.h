// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_DELEGATED_FRAME_DATA_H_
#define CC_DELEGATED_FRAME_DATA_H_

#include "cc/cc_export.h"
#include "cc/render_pass.h"
#include "cc/scoped_ptr_vector.h"
#include "cc/transferable_resource.h"
#include "ui/gfx/size.h"

namespace cc {

class CC_EXPORT DelegatedFrameData {
 public:
  DelegatedFrameData();
  ~DelegatedFrameData();

  TransferableResourceList resource_list;
  ScopedPtrVector<RenderPass> render_pass_list;
};

}  // namespace cc

#endif  // CC_DELEGATED_FRAME_DATA_H_
