// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test_support.h"

#if defined(COMPONENT_BUILD) && defined(COMMAND_BUFFER_GLES_LIB_SUPPORT_ONLY)
bool g_command_buffer_gles_has_atexit_manager;
#endif
