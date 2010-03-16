// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file abstracts differences in logging between NaCl and host
// environment.

#ifndef GPU_COMMAND_BUFFER_COMMON_LOGGING_H_
#define GPU_COMMAND_BUFFER_COMMON_LOGGING_H_

#ifndef __native_client__
#if defined(TRUSTED_GPU_LIBRARY_BUILD)
// Turn off base/logging macros for the trusted library build.
// TODO(dspringer): remove this once building trusted plugins in the Native
// Client SDK is no longer needed.
#define OMIT_DLOG_AND_DCHECK 1
#define GPU_LOG DLOG
#define GPU_CHECK DCHECK
#else
#define GPU_LOG LOG
#define GPU_CHECK CHECK
#endif  // defined(TRUSTED_GPU_LIBRARY_BUILD)
#include "base/logging.h"
#else
#include <sstream>

// TODO: implement logging through nacl's debug service runtime if
// available.
#define CHECK(X) do {} while (0)
#define CHECK_EQ(X, Y) do {} while (0)
#define CHECK_NE(X, Y) do {} while (0)
#define CHECK_GT(X, Y) do {} while (0)
#define CHECK_GE(X, Y) do {} while (0)
#define CHECK_LT(X, Y) do {} while (0)
#define CHECK_LE(X, Y) do {} while (0)

#define DCHECK(X) do {} while (0)
#define DCHECK_EQ(X, Y) do {} while (0)
#define DCHECK_NE(X, Y) do {} while (0)
#define DCHECK_GT(X, Y) do {} while (0)
#define DCHECK_GE(X, Y) do {} while (0)
#define DCHECK_LT(X, Y) do {} while (0)
#define DCHECK_LE(X, Y) do {} while (0)

#define LOG(LEVEL) if (0) std::ostringstream()
#define DLOG(LEVEL) if (0) std::ostringstream()

#define NOTREACHED() DCHECK(false)

#endif

#endif  // GPU_COMMAND_BUFFER_COMMON_LOGGING_H_
