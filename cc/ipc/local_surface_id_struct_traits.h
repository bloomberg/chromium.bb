// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_IPC_LOCAL_SURFACE_ID_STRUCT_TRAITS_H_
#define CC_IPC_LOCAL_SURFACE_ID_STRUCT_TRAITS_H_

#include "cc/ipc/local_surface_id.mojom-shared.h"
#include "cc/surfaces/local_surface_id.h"
#include "mojo/common/common_custom_types_struct_traits.h"

namespace mojo {

template <>
struct StructTraits<cc::mojom::LocalSurfaceIdDataView, cc::LocalSurfaceId> {
  static uint32_t local_id(const cc::LocalSurfaceId& local_surface_id) {
    return local_surface_id.local_id();
  }

  static const base::UnguessableToken& nonce(
      const cc::LocalSurfaceId& local_surface_id) {
    return local_surface_id.nonce();
  }

  static bool Read(cc::mojom::LocalSurfaceIdDataView data,
                   cc::LocalSurfaceId* out) {
    out->local_id_ = data.local_id();
    return data.ReadNonce(&out->nonce_);
  }
};

}  // namespace mojo

#endif  // CC_IPC_LOCAL_SURFACE_ID_STRUCT_TRAITS_H_
