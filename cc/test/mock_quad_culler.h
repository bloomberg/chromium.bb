// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MockCCQuadCuller_h
#define MockCCQuadCuller_h

#include "IntRect.h"
#include "base/memory/scoped_ptr.h"
#include "cc/draw_quad.h"
#include "cc/quad_sink.h"
#include "cc/render_pass.h"

namespace cc {

class MockQuadCuller : public QuadSink {
public:
    MockQuadCuller();
    virtual ~MockQuadCuller();

    MockQuadCuller(QuadList& externalQuadList, SharedQuadStateList& externalSharedQuadStateList);

    virtual bool append(scoped_ptr<DrawQuad> newQuad, AppendQuadsData&) OVERRIDE;

    virtual SharedQuadState* useSharedQuadState(scoped_ptr<SharedQuadState> passSharedQuadState) OVERRIDE;

    const QuadList& quadList() const { return m_activeQuadList; };
    const SharedQuadStateList& sharedQuadStateList() const { return m_activeSharedQuadStateList; };

private:
    QuadList& m_activeQuadList;
    QuadList m_quadListStorage;
    SharedQuadStateList& m_activeSharedQuadStateList;
    SharedQuadStateList m_sharedQuadStateStorage;
};

}  // namespace cc

#endif // MockCCQuadCuller_h
