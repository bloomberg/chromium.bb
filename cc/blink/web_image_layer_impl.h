// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_BLINK_WEB_IMAGE_LAYER_IMPL_H_
#define CC_BLINK_WEB_IMAGE_LAYER_IMPL_H_

#include <memory>

#include "base/macros.h"
#include "cc/blink/cc_blink_export.h"
#include "cc/paint/paint_image.h"
#include "third_party/WebKit/public/platform/WebImageLayer.h"

namespace cc_blink {

class WebLayerImpl;

class WebImageLayerImpl : public blink::WebImageLayer {
 public:
  CC_BLINK_EXPORT WebImageLayerImpl();
  ~WebImageLayerImpl() override;

  // blink::WebImageLayer implementation.
  blink::WebLayer* Layer() override;
  void SetImage(PaintImage image,
                const SkMatrix& matrix,
                bool uses_width_as_height) override;
  void SetNearestNeighbor(bool nearest_neighbor) override;

 private:
  std::unique_ptr<WebLayerImpl> layer_;

  DISALLOW_COPY_AND_ASSIGN(WebImageLayerImpl);
};

}  // namespace cc_blink

#endif  // CC_BLINK_WEB_IMAGE_LAYER_IMPL_H_
