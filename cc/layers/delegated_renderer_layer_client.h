// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_DELEGATED_RENDERER_LAYER_CLIENT_H_
#define CC_LAYERS_DELEGATED_RENDERER_LAYER_CLIENT_H_

#include "cc/base/cc_export.h"

namespace cc {

class CC_EXPORT DelegatedRendererLayerClient {
 public:
  // Called after the object passed in SetFrameData was handed over
  // to the DelegatedRendererLayerImpl.
  virtual void DidCommitFrameData() = 0;

 protected:
  virtual ~DelegatedRendererLayerClient() {}
};

}  // namespace cc
#endif  // CC_LAYERS_DELEGATED_RENDERER_LAYER_CLIENT_H_
