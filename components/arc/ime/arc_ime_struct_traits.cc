// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/ime/arc_ime_struct_traits.h"

namespace mojo {

bool StructTraits<arc::mojom::CursorRectDataView, gfx::Rect>::Read(
    arc::mojom::CursorRectDataView data,
    gfx::Rect* out) {
  out->set_x(data.left());
  out->set_y(data.top());
  out->set_width(data.right() - out->x());
  out->set_height(data.bottom() - out->y());
  return true;
}

bool StructTraits<arc::mojom::TextRangeDataView, gfx::Range>::Read(
    arc::mojom::TextRangeDataView data,
    gfx::Range* out) {
  out->set_start(data.start());
  out->set_end(data.end());
  return true;
}

}  // namespace mojo
