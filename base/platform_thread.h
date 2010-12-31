// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#ifndef BASE_PLATFORM_THREAD_H_
#define BASE_PLATFORM_THREAD_H_
#pragma once

// This file is for backwards-compatibility to keep things compiling that use
// the old location & lack of namespace.
//
// TODO(brettw) make all callers use the new location & namespace and delete
// this file.

#include "base/threading/platform_thread.h"

using base::PlatformThread;
using base::PlatformThreadHandle;
using base::PlatformThreadId;
using base::kNullThreadHandle;
using base::kInvalidThreadId;

#endif  // BASE_PLATFORM_THREAD_H_
