// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_NET_HELIUM_VECTOR_CLOCK_GENERATOR_H_
#define BLIMP_NET_HELIUM_VECTOR_CLOCK_GENERATOR_H_

#include "base/macros.h"
#include "blimp/net/helium/vector_clock.h"

namespace blimp {

// This entity is responsible for generating monotonically increasing values
// of VectorClocks. This generator is going to be used one per host
// (client or engine) to ensure they have a single clock domain.
//
class VectorClockGenerator {
 public:
  VectorClockGenerator() {}

  void Increment() { clock_.IncrementLocal(); }

  const VectorClock& current() { return clock_; }

 private:
  VectorClock clock_;

  DISALLOW_COPY_AND_ASSIGN(VectorClockGenerator);
};

}  // namespace blimp

#endif  // BLIMP_NET_HELIUM_VECTOR_CLOCK_GENERATOR_H_
