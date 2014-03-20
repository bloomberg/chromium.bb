// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/output/overlay_candidate_validator.h"

namespace cc {

OverlayCandidate::OverlayCandidate()
    : transform(NONE),
      format(RGBA_8888),
      uv_rect(0.f, 0.f, 1.f, 1.f),
      overlay_handled(false) {}

OverlayCandidate::~OverlayCandidate() {}

}  // namespace cc
