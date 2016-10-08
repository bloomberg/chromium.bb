// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_NET_HELIUM_VERSION_VECTOR_GENERATOR_H_
#define BLIMP_NET_HELIUM_VERSION_VECTOR_GENERATOR_H_

#include "base/macros.h"
#include "blimp/net/helium/version_vector.h"

namespace blimp {

// Responsible for generating monotonically increasing values
// of VersionVectors. This generator is going to be used one per host
// (client or engine) to ensure they have a single clock domain.
//
class VersionVectorGenerator {
 public:
  VersionVectorGenerator() {}

  void Increment() { clock_.IncrementLocal(); }

  const VersionVector& current() { return clock_; }

 private:
  VersionVector clock_;

  DISALLOW_COPY_AND_ASSIGN(VersionVectorGenerator);
};

}  // namespace blimp

#endif  // BLIMP_NET_HELIUM_VERSION_VECTOR_GENERATOR_H_
