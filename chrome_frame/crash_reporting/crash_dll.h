// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_FRAME_CRASH_REPORTING_CRASH_DLL_H_
#define CHROME_FRAME_CRASH_REPORTING_CRASH_DLL_H_
#pragma once

// Set either of these environment variables to
// crash at load or unload time, respectively.
static const wchar_t kCrashDllName[] = L"crash_dll.dll";
static const wchar_t kCrashOnLoadMode[] = L"CRASH_DLL_CRASH_ON_LOAD";
static const wchar_t kCrashOnUnloadMode[] = L"CRASH_DLL_CRASH_ON_UNLOAD";
static const DWORD kCrashAddress = 0x42;

#endif  // CHROME_FRAME_CRASH_REPORTING_CRASH_DLL_H_
