// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_COMMON_READ_ONLY_BUFFER_STRUCT_TRAITS_H_
#define MOJO_COMMON_READ_ONLY_BUFFER_STRUCT_TRAITS_H_

#include "base/containers/span.h"
#include "mojo/common/read_only_buffer.mojom-shared.h"

namespace mojo {

template <>
struct StructTraits<common::mojom::ReadOnlyBufferDataView,
                    base::span<const uint8_t>> {
  static base::span<const uint8_t> buffer(base::span<const uint8_t> input) {
    return input;
  }

  static bool Read(common::mojom::ReadOnlyBufferDataView input,
                   base::span<const uint8_t>* out) {
    ArrayDataView<uint8_t> data_view;
    input.GetBufferDataView(&data_view);

    // NOTE: This output directly refers to memory owned by the message.
    // Therefore, the message must stay valid while the output is passed to the
    // user code.
    *out = base::span<const uint8_t>(data_view.data(), data_view.size());
    return true;
  }
};

}  // namespace mojo

#endif  // MOJO_COMMON_READ_ONLY_BUFFER_STRUCT_TRAITS_H_
