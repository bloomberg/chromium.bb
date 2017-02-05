// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/ipc/copy_output_request_struct_traits.h"

namespace mojo {

// static
bool StructTraits<cc::mojom::CopyOutputRequestDataView,
                  std::unique_ptr<cc::CopyOutputRequest>>::
    Read(cc::mojom::CopyOutputRequestDataView data,
         std::unique_ptr<cc::CopyOutputRequest>* out_p) {
  auto request = cc::CopyOutputRequest::CreateEmptyRequest();

  request->force_bitmap_result_ = data.force_bitmap_result();

  if (!data.ReadSource(&request->source_))
    return false;

  if (!data.ReadArea(&request->area_))
    return false;

  if (!data.ReadTextureMailbox(&request->texture_mailbox_))
    return false;

  *out_p = std::move(request);

  return true;
}

}  // namespace mojo
