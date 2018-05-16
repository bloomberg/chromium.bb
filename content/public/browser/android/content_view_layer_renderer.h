// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_ANDROID_CONTENT_VIEW_LAYER_RENDERER_H_
#define CONTENT_PUBLIC_BROWSER_ANDROID_CONTENT_VIEW_LAYER_RENDERER_H_

#include "third_party/blink/public/platform/web_layer.h"

namespace content {

// This interface is used by consumers of the ContentViewRenderView to
// attach/detach layers.
class ContentViewLayerRenderer {
 public:
  virtual void AttachLayer(blink::WebLayer* layer) = 0;
  virtual void DetachLayer(blink::WebLayer* layer) = 0;

 protected:
  virtual ~ContentViewLayerRenderer() {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_ANDROID_CONTENT_VIEW_LAYER_RENDERER_H_
