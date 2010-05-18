// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file provides ability to stub LOG and CHECK.

#ifndef GPU_COMMAND_BUFFER_COMMON_LOGGING_H_
#define GPU_COMMAND_BUFFER_COMMON_LOGGING_H_

#if defined(__native_client__)
#define STUB_LOG_AND_CHECK 1
#endif  // __native_client__

#if defined STUB_LOG_AND_CHECK
#include <sstream>

// TODO: implement logging through nacl's debug service runtime if
// available.
#define CHECK(X) if (0) std::ostringstream()
#define CHECK_EQ(X, Y) if (0) std::ostringstream()
#define CHECK_NE(X, Y) if (0) std::ostringstream()
#define CHECK_GT(X, Y) if (0) std::ostringstream()
#define CHECK_GE(X, Y) if (0) std::ostringstream()
#define CHECK_LT(X, Y) if (0) std::ostringstream()
#define CHECK_LE(X, Y) if (0) std::ostringstream()

#define DCHECK(X) if (0) std::ostringstream()
#define DCHECK_EQ(X, Y) if (0) std::ostringstream()
#define DCHECK_NE(X, Y) if (0) std::ostringstream()
#define DCHECK_GT(X, Y) if (0) std::ostringstream()
#define DCHECK_GE(X, Y) if (0) std::ostringstream()
#define DCHECK_LT(X, Y) if (0) std::ostringstream()
#define DCHECK_LE(X, Y) if (0) std::ostringstream()

#define LOG(LEVEL) if (0) std::ostringstream()
#define DLOG(LEVEL) if (0) std::ostringstream()

#define NOTREACHED() DCHECK(false)

#else  // STUB_LOG_AND_CHECK
#include "base/logging.h"
#endif  // STUB_LOG_AND_CHECK

#endif  // GPU_COMMAND_BUFFER_COMMON_LOGGING_H_
