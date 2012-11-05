// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RENDER_PASS_SINK_H_
#define CC_RENDER_PASS_SINK_H_

#include "base/memory/scoped_ptr.h"
#include "cc/cc_export.h"

namespace cc {

class RenderPass;

class CC_EXPORT RenderPassSink {
public:
    virtual void appendRenderPass(scoped_ptr<RenderPass>) = 0;
};

}
#endif  // CC_RENDER_PASS_SINK_H_
