// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEEE_INSTALLER_DLL_INSTALLER_HELPER_H_
#define CEEE_INSTALLER_DLL_INSTALLER_HELPER_H_

// Include file for standard system include files,
// or project specific include files that are used frequently,
// but are changed infrequently

#pragma once

#ifndef STRICT
#define STRICT
#endif

// Modify the following defines if you have to target a platform prior to the
// ones specified below. Refer to MSDN for the latest info on corresponding
// values for different platforms.

// Allow use of features specific to Windows XP or later.
#ifndef WINVER
#define WINVER 0x0501
#endif

// Allow use of features specific to Windows XP or later.
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0501
#endif

// Allow use of features specific to Windows 98 or later.
#ifndef _WIN32_WINDOWS
#define _WIN32_WINDOWS 0x0410
#endif

// Allow use of features specific to IE 6.0 or later.
#ifndef _WIN32_IE
#define _WIN32_IE 0x0600
#endif

#define _ATL_APARTMENT_THREADED
#define _ATL_NO_AUTOMATIC_NAMESPACE

// some CString constructors will be explicit
#define _ATL_CSTRING_EXPLICIT_CONSTRUCTORS


#include "resource.h"
#include <atlbase.h>
#include <atlcom.h>

using namespace ATL;

#endif  // CEEE_INSTALLER_DLL_INSTALLER_HELPER_H_
