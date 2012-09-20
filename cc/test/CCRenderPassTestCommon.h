// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CCRenderPassTestCommon_h
#define CCRenderPassTestCommon_h

#include "CCRenderPass.h"
#include <wtf/PassOwnPtr.h>

namespace WebKitTests {

class CCTestRenderPass : public cc::CCRenderPass {
public:
    cc::CCQuadList& quadList() { return m_quadList; }
    cc::CCSharedQuadStateList& sharedQuadStateList() { return m_sharedQuadStateList; }

    void appendQuad(PassOwnPtr<cc::CCDrawQuad> quad) { m_quadList.append(quad); }
    void appendSharedQuadState(PassOwnPtr<cc::CCSharedQuadState> state) { m_sharedQuadStateList.append(state); }
};

} //namespace WebKitTests

#endif // CCRenderPassTestCommon_h
