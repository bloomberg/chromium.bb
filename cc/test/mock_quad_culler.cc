// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "cc/test/mock_quad_culler.h"

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
    if (!drawQuad->quadRect().isEmpty()) {
        m_activeQuadList.append(drawQuad.Pass());
        return true;
    }
    return false;
}

SharedQuadState* MockQuadCuller::useSharedQuadState(scoped_ptr<SharedQuadState> sharedQuadState)
{
    sharedQuadState->id = m_activeSharedQuadStateList.size();

    SharedQuadState* rawPtr = sharedQuadState.get();
    m_activeSharedQuadStateList.append(sharedQuadState.Pass());
    return rawPtr;
}

} // namespace cc
