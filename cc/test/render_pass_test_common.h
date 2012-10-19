// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CCRenderPassTestCommon_h
#define CCRenderPassTestCommon_h

#include "CCRenderPass.h"

namespace WebKitTests {

class TestRenderPass : public cc::RenderPass {
public:
    cc::QuadList& quadList() { return m_quadList; }
    cc::SharedQuadStateList& sharedQuadStateList() { return m_sharedQuadStateList; }

    void appendQuad(scoped_ptr<cc::DrawQuad> quad) { m_quadList.append(quad.Pass()); }
    void appendSharedQuadState(scoped_ptr<cc::SharedQuadState> state) { m_sharedQuadStateList.append(state.Pass()); }
};

} //namespace WebKitTests

#endif // CCRenderPassTestCommon_h
