// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_IPC_SURFACE_REFERENCE_STRUCT_TRAITS_H_
#define CC_IPC_SURFACE_REFERENCE_STRUCT_TRAITS_H_

#include "cc/ipc/surface_id_struct_traits.h"
#include "cc/ipc/surface_reference.mojom-shared.h"
#include "cc/surfaces/surface_id.h"
#include "cc/surfaces/surface_reference.h"

namespace mojo {

template <>
struct StructTraits<cc::mojom::SurfaceReferenceDataView, cc::SurfaceReference> {
  static const cc::SurfaceId& parent_id(const cc::SurfaceReference& ref) {
    return ref.parent_id();
  }

  static const cc::SurfaceId& child_id(const cc::SurfaceReference& ref) {
    return ref.child_id();
  }

  static bool Read(cc::mojom::SurfaceReferenceDataView data,
                   cc::SurfaceReference* out) {
    return data.ReadParentId(&out->parent_id_) &&
           data.ReadChildId(&out->child_id_);
  }
};

}  // namespace mojo

#endif  // CC_IPC_SURFACE_REFERENCE_STRUCT_TRAITS_H_
