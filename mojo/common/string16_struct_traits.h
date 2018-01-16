// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_COMMON_STRING16_STRUCT_TRAITS_H_
#define MOJO_COMMON_STRING16_STRUCT_TRAITS_H_

#include <cstdint>

#include "base/containers/span.h"
#include "base/strings/string_piece.h"
#include "mojo/common/string16.mojom-shared.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "mojo/public/cpp/bindings/struct_traits.h"

namespace mojo {

template <>
struct StructTraits<common::mojom::String16DataView, base::StringPiece16> {
  static base::span<const uint16_t> data(base::StringPiece16 str) {
    return base::make_span(reinterpret_cast<const uint16_t*>(str.data()),
                           str.size());
  }
};

template <>
struct StructTraits<common::mojom::String16DataView, base::string16> {
  static base::span<const uint16_t> data(const base::string16& str) {
    return StructTraits<common::mojom::String16DataView,
                        base::StringPiece16>::data(str);
  }

  static bool Read(common::mojom::String16DataView data, base::string16* out);
};

template <>
struct StructTraits<common::mojom::BigString16DataView, base::string16> {
  static mojo_base::BigBuffer data(const base::string16& str);

  static bool Read(common::mojom::BigString16DataView data,
                   base::string16* out);
};

}  // namespace mojo

#endif  // MOJO_COMMON_STRING16_STRUCT_TRAITS_H_
