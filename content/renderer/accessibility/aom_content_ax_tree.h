// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_ACCESSIBILITY_AOM_CONTENT_AX_TREE_H_
#define CONTENT_RENDERER_ACCESSIBILITY_AOM_CONTENT_AX_TREE_H_

#include <stdint.h>

#include "third_party/WebKit/public/platform/WebComputedAXTree.h"

#include "base/macros.h"
#include "content/renderer/render_frame_impl.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "ui/accessibility/ax_tree.h"

namespace content {

class AomContentAxTree : public blink::WebComputedAXTree {
 public:
  explicit AomContentAxTree(RenderFrameImpl* render_frame);

  // blink::WebComputedAXTree implementation.
  bool ComputeAccessibilityTree() override;

  // String accessible property getters.
  // TODO(meredithl): Change these to return null rather than empty strings.
  blink::WebString GetNameForAXNode(int32_t) override;
  blink::WebString GetRoleForAXNode(int32_t) override;

  bool GetIntAttributeForAXNode(int32_t axID,
                                blink::WebAOMIntAttribute,
                                int32_t* out_param) override;

 private:
  ui::AXTree tree_;
  RenderFrameImpl* render_frame_;
  DISALLOW_COPY_AND_ASSIGN(AomContentAxTree);
};

}  // namespace content

#endif  // CONTENT_RENDERER_ACCESSIBILITY_AOM_CONTENT_AX_TREE_H_
