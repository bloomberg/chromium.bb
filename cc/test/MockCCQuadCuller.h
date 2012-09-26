// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MockCCQuadCuller_h
#define MockCCQuadCuller_h

#include "CCDrawQuad.h"
#include "CCQuadSink.h"
#include "CCRenderPass.h"
#include "IntRect.h"
#include "base/memory/scoped_ptr.h"

namespace cc {

class MockCCQuadCuller : public CCQuadSink {
public:
    MockCCQuadCuller();
    virtual ~MockCCQuadCuller();

    MockCCQuadCuller(CCQuadList& externalQuadList, CCSharedQuadStateList& externalSharedQuadStateList);

    virtual bool append(scoped_ptr<CCDrawQuad> newQuad, CCAppendQuadsData&) OVERRIDE;

    virtual CCSharedQuadState* useSharedQuadState(scoped_ptr<CCSharedQuadState> passSharedQuadState) OVERRIDE;

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
