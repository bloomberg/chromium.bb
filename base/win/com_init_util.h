// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_WIN_COM_INIT_UTIL_H_
#define BASE_WIN_COM_INIT_UTIL_H_

#include "base/base_export.h"
#include "base/logging.h"

namespace base {
namespace win {

// DCHECKs if COM is not initialized on this thread as an STA or MTA.
#if DCHECK_IS_ON()
BASE_EXPORT void AssertComInitialized();
#else   // DCHECK_IS_ON()
void AssertComInitialized() {}
#endif  // DCHECK_IS_ON()

}  // namespace win
}  // namespace base

#endif  // BASE_WIN_COM_INIT_UTIL_H_
