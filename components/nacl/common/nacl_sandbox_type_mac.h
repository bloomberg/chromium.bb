// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NACL_COMMON_NACL_SANDBOX_TYPE_MAC_H_
#define COMPONENTS_NACL_COMMON_NACL_SANDBOX_TYPE_MAC_H_

#include "content/public/common/sandbox_type_mac.h"

enum NaClSandboxType {
  NACL_SANDBOX_TYPE_FIRST_TYPE = content::SANDBOX_TYPE_AFTER_LAST_TYPE,

  NACL_SANDBOX_TYPE_NACL_LOADER = NACL_SANDBOX_TYPE_FIRST_TYPE,
};

#endif  // COMPONENTS_NACL_COMMON_NACL_SANDBOX_TYPE_MAC_H_
