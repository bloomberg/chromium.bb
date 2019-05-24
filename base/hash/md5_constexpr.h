// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_HASH_MD5_CONSTEXPR_H_
#define BASE_HASH_MD5_CONSTEXPR_H_

#include <array>

#include "base/hash/md5.h"
#include "base/hash/md5_constexpr_internal.h"

namespace base {

// An constexpr implementation of the MD5 hash function. This is no longer
// considered cryptographically secure, but it is useful as a string hashing
// primitive.
//
// This is not the most efficient implementation, so it is not intended to be
// used at runtime. If you do attempt to use it at runtime you will see
// errors about missing symbols.

// Calculates the MD5 digest of the provided data. When passing |string| with
// no explicit length the terminating null will not be processed.
constexpr MD5Digest MD5SumConstexpr(const char* string);
constexpr MD5Digest MD5SumConstexpr(const char* data, uint32_t length);

// Calculates the first 64 bits of the MD5 digest of the provided data, returned
// as a uint64_t. When passing |string| with no explicit length the terminating
// null will not be processed.
constexpr uint64_t MD5HashConstexpr(const char* string);
constexpr uint64_t MD5HashConstexpr(const char* data, uint32_t length);

}  // namespace base

#endif  // BASE_HASH_MD5_CONSTEXPR_H_
