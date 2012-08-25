// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Chromium's LOG() macro collides with one from WTF.
#ifdef LOG
#undef LOG
#endif

#include "base/debug/trace_event.h"
