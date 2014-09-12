// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/public/cpp/surfaces/surfaces_utils.h"

#include "mojo/services/public/cpp/geometry/geometry_type_converters.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/transform.h"

namespace mojo {

SharedQuadStatePtr CreateDefaultSQS(const gfx::Size& size) {
  SharedQuadStatePtr sqs = SharedQuadState::New();
  sqs->content_to_target_transform = Transform::From(gfx::Transform());
  sqs->content_bounds = Size::From(size);
  sqs->visible_content_rect = Rect::From(gfx::Rect(size));
  sqs->clip_rect = Rect::From(gfx::Rect(size));
  sqs->is_clipped = false;
  sqs->opacity = 1.f;
  sqs->blend_mode = mojo::SK_XFERMODE_kSrc_Mode;
  sqs->sorting_context_id = 0;
  return sqs.Pass();
}

PassPtr CreateDefaultPass(int id, const gfx::Rect& rect) {
  PassPtr pass = Pass::New();
  pass->id = id;
  pass->output_rect = Rect::From(rect);
  pass->damage_rect = Rect::From(rect);
  pass->transform_to_root_target = Transform::From(gfx::Transform());
  pass->has_transparent_background = false;
  return pass.Pass();
}

}  // namespace mojo
