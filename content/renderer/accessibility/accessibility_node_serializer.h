// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_ACCESSIBILITY_ACCESSIBILITY_NODE_SERIALIZER_H_
#define CONTENT_RENDERER_ACCESSIBILITY_ACCESSIBILITY_NODE_SERIALIZER_H_

#include "content/common/accessibility_node_data.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebAccessibilityObject.h"

namespace content {

void SerializeAccessibilityNode(
    const WebKit::WebAccessibilityObject& src,
    AccessibilityNodeData* dst,
    bool include_children);

}  // namespace content

#endif  // CONTENT_RENDERER_ACCESSIBILITY_ACCESSIBILITY_NODE_SERIALIZER_H_
