// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OS_WINDOWS
#error Compile this file on Windows only.
#endif

#include "chrome/browser/sync/util/compat-file.h"

#include <windows.h>

const wchar_t* const kPathSeparator = L"\\";

