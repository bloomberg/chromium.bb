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

  MockQuadCuller(QuadList* external_quad_list,
                 SharedQuadStateList* external_shared_quad_state_list);

  // QuadSink interface.
  virtual SharedQuadState* UseSharedQuadState(
      scoped_ptr<SharedQuadState> shared_quad_state) OVERRIDE;
  virtual gfx::Rect UnoccludedContentRect(const gfx::Rect& content_rect,
                                          const gfx::Transform& draw_transform)
      OVERRIDE;
  virtual bool MaybeAppend(scoped_ptr<DrawQuad> draw_quad) OVERRIDE;
  virtual void Append(scoped_ptr<DrawQuad> draw_quad) OVERRIDE;

  const QuadList& quad_list() const { return *active_quad_list_; }
  const SharedQuadStateList& shared_quad_state_list() const {
    return *active_shared_quad_state_list_;
  }

  void set_occluded_content_rect(const gfx::Rect& occluded) {
    occluded_content_rect_ = occluded;
  }

 private:
  QuadList* active_quad_list_;
  QuadList quad_list_storage_;
  SharedQuadStateList* active_shared_quad_state_list_;
  SharedQuadStateList shared_quad_state_storage_;
  gfx::Rect occluded_content_rect_;
};

}  // namespace cc

#endif  // CC_TEST_MOCK_QUAD_CULLER_H_
