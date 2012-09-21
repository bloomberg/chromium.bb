// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "MockCCQuadCuller.h"

namespace cc {

MockCCQuadCuller::MockCCQuadCuller()
    : m_activeQuadList(m_quadListStorage)
    , m_activeSharedQuadStateList(m_sharedQuadStateStorage)
{
}

MockCCQuadCuller::MockCCQuadCuller(CCQuadList& externalQuadList, CCSharedQuadStateList& externalSharedQuadStateList)
    : m_activeQuadList(externalQuadList)
    , m_activeSharedQuadStateList(externalSharedQuadStateList)
{
}

MockCCQuadCuller::~MockCCQuadCuller()
{
}

bool MockCCQuadCuller::append(PassOwnPtr<CCDrawQuad> newQuad, CCAppendQuadsData&)
{
    OwnPtr<CCDrawQuad> drawQuad = newQuad;
    if (!drawQuad->quadRect().isEmpty()) {
        m_activeQuadList.append(drawQuad.release());
        return true;
    }
    return false;
}

CCSharedQuadState* MockCCQuadCuller::useSharedQuadState(PassOwnPtr<CCSharedQuadState> passSharedQuadState)
{
    OwnPtr<CCSharedQuadState> sharedQuadState(passSharedQuadState);
    sharedQuadState->id = m_activeSharedQuadStateList.size();

    CCSharedQuadState* rawPtr = sharedQuadState.get();
    m_activeSharedQuadStateList.append(sharedQuadState.release());
    return rawPtr;
}

} // namespace cc
