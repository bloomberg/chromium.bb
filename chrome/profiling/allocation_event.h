// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_PROFILING_ALLOCATION_EVENT_H_
#define CHROME_PROFILING_ALLOCATION_EVENT_H_

#include <functional>
#include <set>

#include "chrome/profiling/address.h"
#include "chrome/profiling/backtrace_storage.h"

namespace profiling {

// This class is copyable and assignable.
class AllocationEvent {
 public:
  // There must be a reference to this kept in the BacktraceStorage object on
  // behalf of this class.
  AllocationEvent(Address addr, size_t sz, const Backtrace* bt);

  // This partial initializer creates an allocation of empty size for
  // searching purposes.
  explicit AllocationEvent(Address addr);

  Address address() const { return address_; }
  size_t size() const { return size_; }

  // The backtrace for this allocation. There must be a reference to this kept
  // in the BacktraceStorage object on behalf of this class.
  const Backtrace* backtrace() const { return backtrace_; }

  // Implements < for AllocationEvents using address only. This is not a raw
  // operator because it only implements a comparison on the one field.
  struct AddressPartialLess {
    bool operator()(const AllocationEvent& lhs,
                    const AllocationEvent& rhs) const {
      return lhs.address() < rhs.address();
    }
  };

  // Implements == for AllocationEvents using address only. This is not a raw
  // operator because it only implements a comparison on the one field.
  struct AddressPartialEqual {
    bool operator()(const AllocationEvent& lhs,
                    const AllocationEvent& rhs) const {
      return lhs.address() == rhs.address();
    }
  };

 private:
  Address address_;
  size_t size_;
  const Backtrace* backtrace_;
};

using AllocationEventSet =
    std::set<AllocationEvent, AllocationEvent::AddressPartialLess>;

}  // namespace profiling

#endif  // CHROME_PROFILING_ALLOCATION_EVENT_H_
