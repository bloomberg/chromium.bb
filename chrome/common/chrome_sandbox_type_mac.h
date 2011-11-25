// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_CHROME_SANDBOX_TYPE_MAC_H_
#define CHROME_COMMON_CHROME_SANDBOX_TYPE_MAC_H_
#pragma once

#include "content/public/common/sandbox_type_mac.h"

enum ChromeSandboxType {
  CHROME_SANDBOX_TYPE_FIRST_TYPE = content::SANDBOX_TYPE_AFTER_LAST_TYPE,

  CHROME_SANDBOX_TYPE_NACL_LOADER = CHROME_SANDBOX_TYPE_FIRST_TYPE,
};

#endif  // CHROME_COMMON_CHROME_SANDBOX_TYPE_MAC_H_
