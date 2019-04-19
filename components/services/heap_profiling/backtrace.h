// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_HEAP_PROFILING_BACKTRACE_H_
#define COMPONENTS_SERVICES_HEAP_PROFILING_BACKTRACE_H_

#include <functional>
#include <unordered_set>
#include <vector>

#include "base/macros.h"

namespace heap_profiling {

using Address = uint64_t;

// Holds a move-only stack backtrace and a precomputed hash. This backtrace
// uses addresses in the instrumented process. This is in contrast to
// base::StackTrace which is for getting and working with stack traces in the
// current process.
class Backtrace {
 public:
  explicit Backtrace(std::vector<Address>&& a);
  Backtrace(Backtrace&& other) noexcept;
  ~Backtrace();

  Backtrace& operator=(Backtrace&& other);

  bool operator==(const Backtrace& other) const;
  bool operator!=(const Backtrace& other) const;

  const std::vector<Address>& addrs() const { return addrs_; }
  size_t hash() const { return hash_; }

 private:
  std::vector<Address> addrs_;
  size_t hash_;

  DISALLOW_COPY_AND_ASSIGN(Backtrace);
};

using BacktraceStorage = std::unordered_set<Backtrace>;

}  // namespace heap_profiling

namespace std {

template <>
struct hash<heap_profiling::Backtrace> {
  using argument_type = heap_profiling::Backtrace;
  using result_type = size_t;
  result_type operator()(const argument_type& s) const { return s.hash(); }
};

}  // namespace std

#endif  // COMPONENTS_SERVICES_HEAP_PROFILING_BACKTRACE_H_
