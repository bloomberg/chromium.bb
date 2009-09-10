// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if ((!defined(OS_LINUX)) && (!defined(OS_MACOSX)))
#error Compile this file on Mac OS X or Linux only.
#endif

#include "chrome/browser/sync/util/compat-file.h"

const char* const kPathSeparator = "/";

