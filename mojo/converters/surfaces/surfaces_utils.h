// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_CONVERTERS_SURFACES_SURFACES_UTILS_H_
#define MOJO_CONVERTERS_SURFACES_SURFACES_UTILS_H_

#include "components/mus/public/interfaces/quads.mojom.h"
#include "mojo/converters/surfaces/mojo_surfaces_export.h"

namespace gfx {
class Rect;
class Size;
}

namespace mojo {

MOJO_SURFACES_EXPORT mus::mojom::SharedQuadStatePtr CreateDefaultSQS(
    const gfx::Size& size);

// Constructs a pass with the given id, output_rect and damage_rect set to rect,
// transform_to_root_target set to identity and has_transparent_background set
// to false.
MOJO_SURFACES_EXPORT mus::mojom::PassPtr CreateDefaultPass(
    int id,
    const gfx::Rect& rect);

}  // namespace mojo

#endif  // MOJO_CONVERTERS_SURFACES_SURFACES_UTILS_H_
