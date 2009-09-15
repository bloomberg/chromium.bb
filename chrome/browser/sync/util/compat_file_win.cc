// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/util/compat_file.h"

#include "build/build_config.h"

#ifndef OS_WIN
#error Compile this file on Windows only.
#endif

const wchar_t* const kPathSeparator = L"\\";

