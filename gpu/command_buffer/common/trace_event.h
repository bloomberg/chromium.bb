// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_COMMON_TRACE_EVENT_H_
#define GPU_COMMAND_BUFFER_COMMON_TRACE_EVENT_H_
#pragma once

#if !defined(__native_client__)

#include "base/debug/trace_event.h"

#else

#define TRACE_EVENT0(x0, x1) { }
#define TRACE_EVENT1(x0, x1, x2, x3) { }
#define TRACE_EVENT2(x0, x1, x2, x3, x4, x5) { }
#define TRACE_EVENT_INSTANT0(x0, x1) { }
#define TRACE_EVENT_INSTANT1(x0, x1, x2, x3) { }
#define TRACE_EVENT_INSTANT2(x0, x1, x2, x3, x4, x5) { }
#define TRACE_BEGIN0(x0, x1) { }
#define TRACE_BEGIN1(x0, x1, x2, x3) { }
#define TRACE_BEGIN2(x0, x1, x2, x3, x4, x5) { }
#define TRACE_END0(x0, x1) { }
#define TRACE_END1(x0, x1, x2, x3) { }
#define TRACE_END2(x0, x1, x2, x3, x4, x5) { }
#define TRACE_EVENT_IF_LONGER_THAN0(x0, x1, x2) { }
#define TRACE_EVENT_IF_LONGER_THAN1(x0, x1, x2, x3, x4) { }
#define TRACE_EVENT_IF_LONGER_THAN2(x0, x1, x2, x3, x4, x5, x6) { }

#endif  // __native_client__

#endif  // GPU_COMMAND_BUFFER_COMMON_TRACE_EVENT_H_
