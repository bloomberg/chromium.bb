// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CCRenderPassSink_h
#define CCRenderPassSink_h

#include "base/memory/scoped_ptr.h"

namespace cc {

class RenderPass;

class RenderPassSink {
public:
    virtual void appendRenderPass(scoped_ptr<RenderPass>) = 0;
};

}
#endif // CCRenderPassSink_h
