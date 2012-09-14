// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CCRenderPassSink_h
#define CCRenderPassSink_h

#include <wtf/PassOwnPtr.h>

namespace cc {

class CCRenderPass;

class CCRenderPassSink {
public:
    virtual void appendRenderPass(PassOwnPtr<CCRenderPass>) = 0;
};

}
#endif // CCRenderPassSink_h
