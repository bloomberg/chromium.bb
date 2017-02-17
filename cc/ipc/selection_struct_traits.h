// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_IPC_SELECTION_STRUCT_TRAITS_H_
#define CC_IPC_SELECTION_STRUCT_TRAITS_H_

#include "cc/input/selection.h"
#include "cc/ipc/selection.mojom-shared.h"

namespace mojo {

template <>
struct StructTraits<cc::mojom::SelectionDataView,
                    cc::Selection<gfx::SelectionBound>> {
  static const gfx::SelectionBound& start(
      const cc::Selection<gfx::SelectionBound>& selection) {
    return selection.start;
  }

  static const gfx::SelectionBound& end(
      const cc::Selection<gfx::SelectionBound>& selection) {
    return selection.end;
  }

  static bool Read(cc::mojom::SelectionDataView data,
                   cc::Selection<gfx::SelectionBound>* out) {
    return data.ReadStart(&out->start) && data.ReadEnd(&out->end);
  }
};

}  // namespace mojo

#endif  // CC_IPC_SELECTION_STRUCT_TRAITS_H_
