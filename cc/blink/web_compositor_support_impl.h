// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_BLINK_WEB_COMPOSITOR_SUPPORT_IMPL_H_
#define CC_BLINK_WEB_COMPOSITOR_SUPPORT_IMPL_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "cc/blink/cc_blink_export.h"
#include "third_party/blink/public/platform/web_compositor_support.h"
#include "third_party/blink/public/platform/web_layer.h"

namespace cc_blink {

class CC_BLINK_EXPORT WebCompositorSupportImpl
    : public blink::WebCompositorSupport {
 public:
  WebCompositorSupportImpl();
  ~WebCompositorSupportImpl() override;

  std::unique_ptr<blink::WebLayer> CreateLayerFromCCLayer(cc::Layer*) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(WebCompositorSupportImpl);
};

}  // namespace cc_blink

#endif  // CC_BLINK_WEB_COMPOSITOR_SUPPORT_IMPL_H_
