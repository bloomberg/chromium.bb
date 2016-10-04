// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_NET_HELIUM_HELIUM_RESULT_H_
#define BLIMP_NET_HELIUM_HELIUM_RESULT_H_

#include "blimp/net/blimp_net_export.h"

namespace blimp {

// Defines the canonical list of Helium result codes.
// HeliumResult::OK is the only non-error code.
// All other codes are considered errors and are prefixed by ERR_.
// See error_list.h for the unprefixed list of error codes.
//
// (Approach is inspired by net/base/net_errors.h)
enum HeliumResult {
  SUCCESS,
#define HELIUM_ERROR(label, value) ERR_##label = value,
#include "blimp/net/helium/helium_errors.h"
#undef HELIUM_ERROR
};

// Gets a human-readable string representation of |result|.
const char* BLIMP_NET_EXPORT HeliumResultToString(HeliumResult result);

}  // namespace blimp

#endif  // BLIMP_NET_HELIUM_HELIUM_RESULT_H_
