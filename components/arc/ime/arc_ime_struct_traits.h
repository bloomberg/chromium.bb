// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_IME_ARC_IME_STRUCT_TRAITS_H_
#define COMPONENTS_ARC_IME_ARC_IME_STRUCT_TRAITS_H_

#include "components/arc/common/ime.mojom.h"
#include "ui/gfx/geometry/rect.h"

namespace mojo {

template <>
struct StructTraits<arc::mojom::CursorRectDataView, gfx::Rect> {
  static int32_t left(const gfx::Rect& r) { return r.x(); }
  static int32_t top(const gfx::Rect& r) { return r.y(); }
  static int32_t right(const gfx::Rect& r) { return r.right(); }
  static int32_t bottom(const gfx::Rect& r) { return r.bottom(); }

  static bool Read(arc::mojom::CursorRectDataView data, gfx::Rect* out);
};

template <>
struct StructTraits<arc::mojom::TextRangeDataView, gfx::Range> {
  static uint32_t start(const gfx::Range& r) { return r.start(); }
  static uint32_t end(const gfx::Range& r) { return r.end(); }

  static bool Read(arc::mojom::TextRangeDataView data, gfx::Range* out);
};

}  // namespace mojo

#endif  // COMPONENTS_ARC_IME_ARC_IME_STRUCT_TRAITS_H_
