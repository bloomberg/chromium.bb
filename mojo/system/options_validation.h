// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Functions to help with verifying various |Mojo...Options| structs from the
// (public, C) API. These are "extensible" structs, which all have |struct_size|
// as their first member. All fields (other than |struct_size|) are optional,
// but any |flags| specified must be known to the system (otherwise, an error of
// |MOJO_RESULT_UNIMPLEMENTED| should be returned).

#ifndef MOJO_SYSTEM_OPTIONS_VALIDATION_H_
#define MOJO_SYSTEM_OPTIONS_VALIDATION_H_

#include <stddef.h>
#include <stdint.h>

#include <algorithm>

#include "base/logging.h"
#include "base/macros.h"
#include "mojo/public/c/system/types.h"
#include "mojo/system/constants.h"
#include "mojo/system/memory.h"
#include "mojo/system/system_impl_export.h"

namespace mojo {
namespace system {

template <class Options>
class UserOptionsReader {
 public:
  // Constructor from a |UserPointer<const Options>| (which it checks -- this
  // constructor has side effects!).
  // Note: We initialize |options_reader_| without checking, since we do a check
  // in |GetSizeForReader()|.
  explicit UserOptionsReader(UserPointer<const Options> options)
      : options_reader_(UserPointer<const char>::Reader::NoCheck(),
                        options.template ReinterpretCast<const char>(),
                        GetSizeForReader(options)) {
    COMPILE_ASSERT(offsetof(Options, struct_size) == 0,
                   Options_struct_size_not_first_member);
    // TODO(vtl): With C++11, compile-assert that |sizeof(Options::struct_size)
    // == sizeof(uint32_t)| somewhere.
  }

  bool is_valid() const { return !!options_reader_.GetPointer(); }

  const Options& options() const {
    DCHECK(is_valid());
    return *reinterpret_cast<const Options*>(options_reader_.GetPointer());
  }

  // Checks that the given (variable-size) |options| passed to the constructor
  // (plausibly) has a member at the given offset with the given size. You
  // probably want to use |OPTIONS_STRUCT_HAS_MEMBER()| instead.
  bool HasMember(size_t offset, size_t size) const {
    DCHECK(is_valid());
    // We assume that |offset| and |size| are reasonable, since they should come
    // from |offsetof(Options, some_member)| and |sizeof(Options::some_member)|,
    // respectively.
    return options().struct_size >= offset + size;
  }

 private:
  static inline size_t GetSizeForReader(UserPointer<const Options> options) {
    uint32_t struct_size =
        options.template ReinterpretCast<const uint32_t>().Get();
    if (struct_size < sizeof(uint32_t))
      return 0;

    // Check the full requested size.
    // Note: Use |MOJO_ALIGNOF()| here to match the exact macro used in the
    // declaration of Options structs.
    internal::CheckUserPointerWithSize<MOJO_ALIGNOF(Options)>(options.pointer_,
                                                              struct_size);
    options.template ReinterpretCast<const char>().CheckArray(struct_size);
    // But we'll never look at more than |sizeof(Options)| bytes.
    return std::min(static_cast<size_t>(struct_size), sizeof(Options));
  }

  UserPointer<const char>::Reader options_reader_;

  DISALLOW_COPY_AND_ASSIGN(UserOptionsReader);
};

// Macro to invoke |UserOptionsReader<Options>::HasMember()| parametrized by
// member name instead of offset and size.
//
// (We can't just give |HasMember()| a member pointer template argument instead,
// since there's no good/strictly-correct way to get an offset from that.)
//
// TODO(vtl): With C++11, use |sizeof(Options::member)| instead of (the
// contortion below). We might also be able to pull out the type |Options| from
// |reader| (using |decltype|) instead of requiring a parameter.
#define OPTIONS_STRUCT_HAS_MEMBER(Options, member, reader) \
  reader.HasMember(offsetof(Options, member), sizeof(reader.options().member))

}  // namespace system
}  // namespace mojo

#endif  // MOJO_SYSTEM_OPTIONS_VALIDATION_H_
