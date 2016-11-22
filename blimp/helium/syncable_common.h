// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_HELIUM_SYNCABLE_COMMON_H_
#define BLIMP_HELIUM_SYNCABLE_COMMON_H_

#include "blimp/helium/blimp_helium_export.h"

namespace blimp {
namespace helium {

enum class Peer { CLIENT, ENGINE };

enum class Bias { LOCAL, REMOTE };

// Determines whether a Syncable should be biased toward local or remote
// mutations in the event of a conflict.
Bias BLIMP_HELIUM_EXPORT ComputeBias(Peer owner, Peer running_as);

}  // namespace helium
}  // namespace blimp

#endif  // BLIMP_HELIUM_SYNCABLE_COMMON_H_
