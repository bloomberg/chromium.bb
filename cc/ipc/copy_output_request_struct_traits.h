// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_IPC_COPY_OUTPUT_REQUEST_STRUCT_TRAITS_H_
#define CC_IPC_COPY_OUTPUT_REQUEST_STRUCT_TRAITS_H_

#include "cc/ipc/copy_output_request.mojom-shared.h"
#include "cc/output/copy_output_request.h"

namespace mojo {

template <>
struct StructTraits<cc::mojom::CopyOutputRequestDataView,
                    cc::CopyOutputRequest> {
  static const base::Optional<base::UnguessableToken>& source(
      const cc::CopyOutputRequest& request) {
    return request.source_;
  }

  static bool force_bitmap_result(const cc::CopyOutputRequest& request) {
    return request.force_bitmap_result_;
  }

  static const base::Optional<gfx::Rect>& area(
      const cc::CopyOutputRequest& request) {
    return request.area_;
  }

  static const base::Optional<cc::TextureMailbox>& texture_mailbox(
      const cc::CopyOutputRequest& request) {
    return request.texture_mailbox_;
  }

  static bool Read(cc::mojom::CopyOutputRequestDataView data,
                   cc::CopyOutputRequest* out) {
    out->force_bitmap_result_ = data.force_bitmap_result();
    return data.ReadSource(&out->source_) && data.ReadArea(&out->area_) &&
           data.ReadTextureMailbox(&out->texture_mailbox_);
  }
};

}  // namespace mojo

#endif  // CC_IPC_COPY_OUTPUT_REQUEST_STRUCT_TRAITS_H_
