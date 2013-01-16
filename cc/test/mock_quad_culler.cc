// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/mock_quad_culler.h"

#include "cc/draw_quad.h"

namespace cc {

MockQuadCuller::MockQuadCuller()
    : m_activeQuadList(m_quadListStorage)
    , m_activeSharedQuadStateList(m_sharedQuadStateStorage)
{
}

MockQuadCuller::MockQuadCuller(QuadList& externalQuadList, SharedQuadStateList& externalSharedQuadStateList)
    : m_activeQuadList(externalQuadList)
    , m_activeSharedQuadStateList(externalSharedQuadStateList)
{
}

MockQuadCuller::~MockQuadCuller()
{
}

bool MockQuadCuller::append(scoped_ptr<DrawQuad> drawQuad, AppendQuadsData&)
{
    if (!drawQuad->rect.IsEmpty()) {
        m_activeQuadList.push_back(drawQuad.Pass());
        return true;
    }
    return false;
}

SharedQuadState* MockQuadCuller::useSharedQuadState(scoped_ptr<SharedQuadState> sharedQuadState)
{
    SharedQuadState* rawPtr = sharedQuadState.get();
    m_activeSharedQuadStateList.push_back(sharedQuadState.Pass());
    return rawPtr;
}

}  // namespace cc
