// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_MOCK_QUAD_CULLER_H_
#define CC_TEST_MOCK_QUAD_CULLER_H_

#include "base/memory/scoped_ptr.h"
#include "cc/layers/quad_sink.h"
#include "cc/quads/draw_quad.h"
#include "cc/quads/render_pass.h"

namespace cc {

class MockQuadCuller : public QuadSink {
public:
    MockQuadCuller();
    virtual ~MockQuadCuller();

    MockQuadCuller(QuadList& externalQuadList, SharedQuadStateList& externalSharedQuadStateList);

    virtual bool Append(scoped_ptr<DrawQuad> draw_quad, AppendQuadsData* append_quads_data) OVERRIDE;

    virtual SharedQuadState* UseSharedQuadState(scoped_ptr<SharedQuadState> shared_quad_state) OVERRIDE;

    const QuadList& quadList() const { return m_activeQuadList; };
    const SharedQuadStateList& sharedQuadStateList() const { return m_activeSharedQuadStateList; };

private:
    QuadList& m_activeQuadList;
    QuadList m_quadListStorage;
    SharedQuadStateList& m_activeSharedQuadStateList;
    SharedQuadStateList m_sharedQuadStateStorage;
};

}  // namespace cc

#endif  // CC_TEST_MOCK_QUAD_CULLER_H_
