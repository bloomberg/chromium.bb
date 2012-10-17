// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_DCHECK_H_
#define CC_DCHECK_H_

#include "base/logging.h"

// TODO(danakj): Move this into base/logging.

#if !LOGGING_IS_OFFICIAL_BUILD
#define CC_DCHECK_ENABLED() 1
#else
#define CC_DCHECK_ENABLED() 0
#endif

#endif // CC_DCHECK_H_
