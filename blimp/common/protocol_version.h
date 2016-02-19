// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_COMMON_PROTOCOL_VERSION_H_
#define BLIMP_COMMON_PROTOCOL_VERSION_H_

#include "blimp/common/blimp_common_export.h"

namespace blimp {

const int BLIMP_COMMON_EXPORT kProtocolVersion = 0;

// TODO(dtrainor): Move this out or integrate this with kProtocolVersion?
const char BLIMP_COMMON_EXPORT kEngineVersion[] = "20160209094838";

}  // namespace blimp

#endif  // BLIMP_COMMON_PROTOCOL_VERSION_H_
