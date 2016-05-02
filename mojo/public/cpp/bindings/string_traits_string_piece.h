// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_STRING_TRAITS_STRING_PIECE_H_
#define MOJO_PUBLIC_CPP_BINDINGS_STRING_TRAITS_STRING_PIECE_H_

#include "base/strings/string_piece.h"
#include "mojo/public/cpp/bindings/string_traits.h"

namespace mojo {

template <>
struct StringTraits<base::StringPiece> {
  // No IsNull() function, which means that base::StringPiece is always
  // considered as non-null mojom string. We could have let StringPiece
  // containing a null data pointer map to null mojom string, but
  // StringPiece::empty() returns true in this case. It seems confusing to mix
  // the concept of empty and null strings, especially because they mean
  // different things in mojom.

  static size_t GetSize(const base::StringPiece& input) { return input.size(); }

  static const char* GetData(const base::StringPiece& input) {
    return input.data();
  }

  // TODO(yzshen): Use a public type, such as mojo::String::DataView, for
  // |input|.
  static bool Read(internal::String_Data* input, base::StringPiece* output) {
    if (input)
      output->set(input->storage(), input->size());
    else
      output->set(nullptr, 0);
    return true;
  }
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_STRING_TRAITS_STRING_PIECE_H_
