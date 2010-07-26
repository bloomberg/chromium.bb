// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_NSS_SLOT_UTIL_H_
#define BASE_NSS_SLOT_UTIL_H_
#pragma once

#include <secmodt.h>

// These functions return a type defined in an NSS header, and so cannot be
// declared in nss_util.h.  Hence, they are declared here.

namespace base {

// Returns a reference to the default NSS key slot. Caller must release
// reference with PK11_FreeSlot.
PK11SlotInfo* GetDefaultNSSKeySlot();

}  // namespace base

#endif  // BASE_NSS_UTIL_H_
