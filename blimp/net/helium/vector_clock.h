// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_NET_HELIUM_VECTOR_CLOCK_H_
#define BLIMP_NET_HELIUM_VECTOR_CLOCK_H_

#include <stdint.h>

#include "blimp/net/blimp_net_export.h"

namespace blimp {

// From wikipedia:
// A vector clock is an algorithm for generating a partial ordering of events
// in a distributed system and detecting causality violations. This is used
// in Blimp to allow client and server modify a local copy of an object and
// later be able to detect ordering or conflicts if any.
//
// For more info see:
// https://en.wikipedia.org/wiki/Vector_clock

typedef uint32_t Revision;

class BLIMP_NET_EXPORT VectorClock {
 public:
  enum class Comparison { LessThan, EqualTo, GreaterThan, Conflict };

  VectorClock(Revision local_revision, Revision remote_revision);
  VectorClock();

  // Compares two vector clocks. There are 4 possibilities for the result:
  // * LessThan: One revision is equal and for the other is smaller.
  // (1,0).CompareTo((2, 0));
  // * EqualTo: Both revisions are the same.
  // * GreaterThan: One revision is equal and for the other is greater.
  // (2,0).CompareTo((1, 0));
  // * Conflict: Both revisions are different. (1,0).CompareTo(0,1)
  Comparison CompareTo(const VectorClock& other) const;

  // Merges two vector clocks. This function should be used at synchronization
  // points. i.e. when client receives data from the server or vice versa.
  VectorClock MergeWith(const VectorClock& other) const;

  // Increments local_revision_ by one. This is used when something changes
  // in the local state like setting a property or applying a change set.
  void IncrementLocal();

  Revision local_revision() const { return local_revision_; }

  void set_local_revision(Revision local_revision) {
    local_revision_ = local_revision;
  }

  Revision remote_revision() const { return remote_revision_; }

  void set_remote_revision(Revision remote_revision) {
    remote_revision_ = remote_revision;
  }

 private:
  Revision local_revision_ = 0;
  Revision remote_revision_ = 0;
};

}  // namespace blimp

#endif  // BLIMP_NET_HELIUM_VECTOR_CLOCK_H_
