// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/ipc/copy_output_result_struct_traits.h"

namespace mojo {

// static
bool StructTraits<cc::mojom::CopyOutputResultDataView, cc::CopyOutputResult>::
    Read(cc::mojom::CopyOutputResultDataView data, cc::CopyOutputResult* out) {
  out->bitmap_ = base::MakeUnique<SkBitmap>();
  return data.ReadSize(&out->size_) && data.ReadBitmap(out->bitmap_.get()) &&
         data.ReadTextureMailbox(&out->texture_mailbox_);
}

// static
const SkBitmap&
StructTraits<cc::mojom::CopyOutputResultDataView, cc::CopyOutputResult>::bitmap(
    const cc::CopyOutputResult& result) {
  static SkBitmap* null_bitmap = new SkBitmap();
  if (!result.bitmap_)
    return *null_bitmap;
  return *result.bitmap_;
}

}  // namespace mojo
