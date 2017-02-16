// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_IPC_COPY_OUTPUT_REQUEST_STRUCT_TRAITS_H_
#define CC_IPC_COPY_OUTPUT_REQUEST_STRUCT_TRAITS_H_

#include "cc/ipc/copy_output_request.mojom.h"
#include "cc/ipc/texture_mailbox_struct_traits.h"
#include "cc/output/copy_output_request.h"
#include "mojo/common/common_custom_types_struct_traits.h"
#include "ui/gfx/geometry/mojo/geometry_struct_traits.h"

namespace mojo {

template <>
struct StructTraits<cc::mojom::CopyOutputRequestDataView,
                    std::unique_ptr<cc::CopyOutputRequest>> {
  static const base::Optional<base::UnguessableToken>& source(
      const std::unique_ptr<cc::CopyOutputRequest>& request) {
    return request->source_;
  }

  static bool force_bitmap_result(
      const std::unique_ptr<cc::CopyOutputRequest>& request) {
    return request->force_bitmap_result_;
  }

  static const base::Optional<gfx::Rect>& area(
      const std::unique_ptr<cc::CopyOutputRequest>& request) {
    return request->area_;
  }

  static const base::Optional<cc::TextureMailbox>& texture_mailbox(
      const std::unique_ptr<cc::CopyOutputRequest>& request) {
    return request->texture_mailbox_;
  }

  static cc::mojom::CopyOutputResultSenderPtr result_sender(
      const std::unique_ptr<cc::CopyOutputRequest>& request);

  static bool Read(cc::mojom::CopyOutputRequestDataView data,
                   std::unique_ptr<cc::CopyOutputRequest>* out_p);
};

}  // namespace mojo

#endif  // CC_IPC_COPY_OUTPUT_REQUEST_STRUCT_TRAITS_H_
