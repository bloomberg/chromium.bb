// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/net/helium/helium_result.h"

#include "base/logging.h"

namespace blimp {

const char* HeliumResultToString(HeliumResult result) {
  switch (result) {
    case HeliumResult::SUCCESS:
      return "SUCCESS";
      break;
#define HELIUM_ERROR(label, value) \
  case HeliumResult::ERR_##label:  \
    return "ERR_" #label;
      break;
#include "blimp/net/helium/helium_errors.h"
#undef HELIUM_ERROR
  }

  NOTREACHED();
  return "";
}

}  // namespace blimp
