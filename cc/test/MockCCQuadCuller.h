// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MockCCQuadCuller_h
#define MockCCQuadCuller_h

#include "CCDrawQuad.h"
#include "CCQuadSink.h"
#include "IntRect.h"
#include <wtf/PassOwnPtr.h>

namespace cc {

class MockCCQuadCuller : public CCQuadSink {
public:
    MockCCQuadCuller()
        : m_activeQuadList(m_quadListStorage)
        , m_activeSharedQuadStateList(m_sharedQuadStateStorage)
    { }

    explicit MockCCQuadCuller(CCQuadList& externalQuadList, CCSharedQuadStateList& externalSharedQuadStateList)
        : m_activeQuadList(externalQuadList)
        , m_activeSharedQuadStateList(externalSharedQuadStateList)
    { }

    virtual bool append(PassOwnPtr<CCDrawQuad> newQuad, CCAppendQuadsData&) OVERRIDE
    {
        OwnPtr<CCDrawQuad> drawQuad = newQuad;
        if (!drawQuad->quadRect().isEmpty()) {
            m_activeQuadList.append(drawQuad.release());
            return true;
        }
        return false;
    }

    virtual CCSharedQuadState* useSharedQuadState(PassOwnPtr<CCSharedQuadState> passSharedQuadState) OVERRIDE
    {
        OwnPtr<CCSharedQuadState> sharedQuadState(passSharedQuadState);
        sharedQuadState->id = m_activeSharedQuadStateList.size();

        CCSharedQuadState* rawPtr = sharedQuadState.get();
        m_activeSharedQuadStateList.append(sharedQuadState.release());
        return rawPtr;
    }

    const CCQuadList& quadList() const { return m_activeQuadList; };
    const CCSharedQuadStateList& sharedQuadStateList() const { return m_activeSharedQuadStateList; };

private:
    CCQuadList& m_activeQuadList;
    CCQuadList m_quadListStorage;
    CCSharedQuadStateList& m_activeSharedQuadStateList;
    CCSharedQuadStateList m_sharedQuadStateStorage;
};

} // namespace cc
#endif // MockCCQuadCuller_h
