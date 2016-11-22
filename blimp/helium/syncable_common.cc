// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/helium/syncable_common.h"

namespace blimp {
namespace helium {

Bias ComputeBias(Peer owner, Peer running_as) {
  return (owner == running_as ? Bias::LOCAL : Bias::REMOTE);
}

}  // namespace helium
}  // namespace blimp
