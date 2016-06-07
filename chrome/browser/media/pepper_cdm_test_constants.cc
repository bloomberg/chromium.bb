// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/pepper_cdm_test_constants.h"

#include "build/build_config.h"

const char kClearKeyCdmBaseDirectory[] = "ClearKeyCdm";

const char kClearKeyCdmAdapterFileName[] =
#if defined(OS_MACOSX)
    "clearkeycdmadapter.plugin";
#elif defined(OS_WIN)
    "clearkeycdmadapter.dll";
#elif defined(OS_POSIX)
    "libclearkeycdmadapter.so";
#endif

const char kClearKeyCdmDisplayName[] = "Clear Key CDM";

const char kClearKeyCdmPepperMimeType[] = "application/x-ppapi-clearkey-cdm";
