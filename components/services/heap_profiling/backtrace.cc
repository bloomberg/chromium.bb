// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/heap_profiling/backtrace.h"

#include <cstring>
#include <utility>

#include "base/hash/hash.h"

namespace heap_profiling {

namespace {
size_t ComputeHash(const std::vector<Address>& addrs) {
  if (addrs.empty())
    return 0;
  static_assert(std::is_integral<Address>::value,
                "base::Hash call below needs simple type.");
  return base::Hash(addrs.data(), addrs.size() * sizeof(Address));
}
}  // namespace

Backtrace::Backtrace(std::vector<Address>&& a)
    : addrs_(std::move(a)), hash_(ComputeHash(addrs_)) {}

Backtrace::Backtrace(Backtrace&& other) noexcept = default;

Backtrace::~Backtrace() = default;

Backtrace& Backtrace::operator=(Backtrace&& other) = default;

bool Backtrace::operator==(const Backtrace& other) const {
  if (addrs_.size() != other.addrs_.size())
    return false;
  return memcmp(addrs_.data(), other.addrs_.data(),
                addrs_.size() * sizeof(Address)) == 0;
}

bool Backtrace::operator!=(const Backtrace& other) const {
  return !operator==(other);
}

}  // namespace heap_profiling
