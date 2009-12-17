// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file abstracts differences in logging between NaCl and host
// environment.

#ifndef GPU_COMMAND_BUFFER_COMMON_LOGGING_H_
#define GPU_COMMAND_BUFFER_COMMON_LOGGING_H_

#ifndef __native_client__
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

#endif

#endif  // GPU_COMMAND_BUFFER_COMMON_LOGGING_H_
