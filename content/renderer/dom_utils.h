// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_DOM_UTILS_H_
#define CONTENT_RENDERER_DOM_UTILS_H_

namespace blink {
class WebNode;
}

namespace content {

class DomUtils {
 public:
  // Walks up the DOM, looking for the first parent that represents an <a>.
  // Returns a null WebNode if no such <a> exists.
  static blink::WebNode ExtractParentAnchorNode(const blink::WebNode& node);
};

}  // namespace content

#endif  // CONTENT_RENDERER_DOM_UTILS_H_
