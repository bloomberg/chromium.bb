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

#include "base/macros.h"
#include "mojo/system/memory.h"
#include "mojo/system/system_impl_export.h"

namespace mojo {
namespace system {

// Checks that |buffer| appears to contain a valid Options struct, namely
// properly aligned and with a |struct_size| field (which must the first field
// of the struct and be a |uint32_t|) containing a plausible size.
template <class Options>
bool IsOptionsStructPointerAndSizeValid(const void* buffer) {
  COMPILE_ASSERT(offsetof(Options, struct_size) == 0,
                 Options_struct_size_not_first_member);
  // TODO(vtl): With C++11, use |sizeof(Options::struct_size)| instead.
  COMPILE_ASSERT(sizeof(static_cast<const Options*>(buffer)->struct_size) ==
                     sizeof(uint32_t),
                 Options_struct_size_not_32_bits);

  // Note: Use |MOJO_ALIGNOF()| here to match the exact macro used in the
  // declaration of Options structs.
  if (!internal::VerifyUserPointerHelper<sizeof(uint32_t),
                                         MOJO_ALIGNOF(Options)>(buffer))
    return false;

  return static_cast<const Options*>(buffer)->struct_size >= sizeof(uint32_t);
}

// Checks that the Options struct in |buffer| has a member with the given offset
// and size. This may be called only if |IsOptionsStructPointerAndSizeValid()|
// returned true.
//
// You may want to use the macro |HAS_OPTIONS_STRUCT_MEMBER()| instead.
template <class Options, size_t offset, size_t size>
bool HasOptionsStructMember(const void* buffer) {
  // We assume that |offset| and |size| are reasonable, since they should come
  // from |offsetof(Options, some_member)| and |sizeof(Options::some_member)|,
  // respectively.
  return static_cast<const Options*>(buffer)->struct_size >=
      offset + size;
}

// Macro to invoke |HasOptionsStructMember()| parametrized by member name
// instead of offset and size.
//
// (We can't just give |HasOptionsStructMember()| a member pointer template
// argument instead, since there's no good/strictly-correct way to get an offset
// from that.)
//
// TODO(vtl): With C++11, use |sizeof(Options::member)| instead.
#define HAS_OPTIONS_STRUCT_MEMBER(Options, member, buffer) \
    (HasOptionsStructMember< \
        Options, \
        offsetof(Options, member), \
        sizeof(static_cast<const Options*>(buffer)->member)>(buffer))

// Checks that the (standard) |flags| member consists of only known flags. This
// should only be called if |HAS_OPTIONS_STRUCT_MEMBER()| returned true for the
// |flags| field.
//
// The rationale for *not* ignoring these flags is that the caller should have a
// way of specifying that certain options not be ignored. E.g., one may have a
// |MOJO_..._OPTIONS_FLAG_DONT_IGNORE_FOO| flag and a |foo| member; if the flag
// is set, it will guarantee that the version of the system knows about the
// |foo| member (and won't ignore it).
template <class Options>
bool AreOptionsFlagsAllKnown(const void* buffer, uint32_t known_flags) {
  return (static_cast<const Options*>(buffer)->flags & ~known_flags) == 0;
}

}  // namespace system
}  // namespace mojo

#endif  // MOJO_SYSTEM_OPTIONS_VALIDATION_H_
