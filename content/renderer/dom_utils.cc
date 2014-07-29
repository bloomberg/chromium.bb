// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/dom_utils.h"

#include "third_party/WebKit/public/web/WebNode.h"

namespace content {

blink::WebNode DomUtils::ExtractParentAnchorNode(
    const blink::WebNode& node) {
  blink::WebNode selected_node = node;

  // If there are other embedded tags (like <a ..>Some <b>text</b></a>)
  // we need to extract the parent <a/> node.
  while (!selected_node.isNull() && !selected_node.isLink())
    selected_node = selected_node.parentNode();
  return selected_node;
}

}  // namespace content
