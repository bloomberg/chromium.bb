// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_IPC_COPY_OUTPUT_RESULT_STRUCT_TRAITS_H_
#define CC_IPC_COPY_OUTPUT_RESULT_STRUCT_TRAITS_H_

#include "cc/ipc/copy_output_result.mojom-shared.h"
#include "cc/ipc/texture_mailbox_releaser.mojom.h"
#include "cc/ipc/texture_mailbox_struct_traits.h"
#include "cc/output/copy_output_result.h"
#include "skia/public/interfaces/bitmap_skbitmap_struct_traits.h"
#include "ui/gfx/geometry/mojo/geometry_struct_traits.h"

namespace mojo {

template <>
struct StructTraits<cc::mojom::CopyOutputResultDataView,
                    std::unique_ptr<cc::CopyOutputResult>> {
  static const gfx::Size& size(
      const std::unique_ptr<cc::CopyOutputResult>& result) {
    return result->size_;
  }

  static const SkBitmap& bitmap(
      const std::unique_ptr<cc::CopyOutputResult>& result);

  static const cc::TextureMailbox& texture_mailbox(
      const std::unique_ptr<cc::CopyOutputResult>& result) {
    return result->texture_mailbox_;
  }

  static cc::mojom::TextureMailboxReleaserPtr releaser(
      const std::unique_ptr<cc::CopyOutputResult>& result);

  static bool Read(cc::mojom::CopyOutputResultDataView data,
                   std::unique_ptr<cc::CopyOutputResult>* out_p);
};

}  // namespace mojo

#endif  // CC_IPC_COPY_OUTPUT_RESULT_STRUCT_TRAITS_H_
