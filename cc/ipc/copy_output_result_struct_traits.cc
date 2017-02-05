// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/ipc/copy_output_result_struct_traits.h"

namespace mojo {

// static
bool StructTraits<cc::mojom::CopyOutputResultDataView,
                  std::unique_ptr<cc::CopyOutputResult>>::
    Read(cc::mojom::CopyOutputResultDataView data,
         std::unique_ptr<cc::CopyOutputResult>* out_p) {
  auto result = cc::CopyOutputResult::CreateEmptyResult();

  if (!data.ReadSize(&result->size_))
    return false;

  result->bitmap_ = base::MakeUnique<SkBitmap>();
  if (!data.ReadBitmap(result->bitmap_.get()))
    return false;

  if (!data.ReadTextureMailbox(&result->texture_mailbox_))
    return false;

  *out_p = std::move(result);

  return true;
}

// static
const SkBitmap& StructTraits<cc::mojom::CopyOutputResultDataView,
                             std::unique_ptr<cc::CopyOutputResult>>::
    bitmap(const std::unique_ptr<cc::CopyOutputResult>& result) {
  static SkBitmap* null_bitmap = new SkBitmap();
  if (!result->bitmap_)
    return *null_bitmap;
  return *result->bitmap_;
}

}  // namespace mojo
