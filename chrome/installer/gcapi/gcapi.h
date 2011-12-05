// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_GCAPI_GCAPI_H_
#define CHROME_INSTALLER_GCAPI_GCAPI_H_
#pragma once

#include <windows.h>

extern "C" {
// Error conditions for GoogleChromeCompatibilityCheck().
#define GCCC_ERROR_USERLEVELALREADYPRESENT       0x01
#define GCCC_ERROR_SYSTEMLEVELALREADYPRESENT     0x02
#define GCCC_ERROR_ACCESSDENIED                  0x04
#define GCCC_ERROR_OSNOTSUPPORTED                0x08
#define GCCC_ERROR_ALREADYOFFERED                0x10
#define GCCC_ERROR_INTEGRITYLEVEL                0x20

// This function returns TRUE if Google Chrome should be offered.
// If the return is FALSE, the reasons DWORD explains why.  If you don't care
// for the reason, you can pass NULL for reasons.
// set_flag indicates whether a flag should be set indicating that Chrome was
// offered within the last six months; if passed FALSE, this method will not
// set the flag even if Chrome can be offered.  If passed TRUE, this method
// will set the flag only if Chrome can be offered.
BOOL __stdcall GoogleChromeCompatibilityCheck(BOOL set_flag, DWORD* reasons);

// This function launches Google Chrome after a successful install. Make
// sure COM library is NOT initialized before you call this function (so if
// you called CoInitialize, call CoUninitialize before calling this function).
BOOL __stdcall LaunchGoogleChrome();

// This function launches Google Chrome after a successful install at the
// given x,y coordinates with size height,length. Set in_background to true
// to move Google Chrome behind all other windows or false to have it appear
// at the default z-order. Make sure that COM is NOT initialized before you call
// this function (so if you called CoInitialize, call CoUninitialize before
// calling this function).
// This call is synchronous, meaning it waits for Chrome to launch and appear
// to resize it before returning.
BOOL __stdcall LaunchGoogleChromeWithDimensions(int x,
                                                int y,
                                                int width,
                                                int height,
                                                bool in_background);

// This function returns the number of days since Google Chrome was last run by
// the current user. If both user-level and machine-wide installations are
// present on the system, it will return the lowest last-run-days count of
// the two.
// Returns -1 if Chrome is not installed, the last run date is in the future,
// or we are otherwise unable to determine how long since Chrome was last
// launched.
int __stdcall GoogleChromeDaysSinceLastRun();

// Funtion pointer type declarations to use with GetProcAddress.
typedef BOOL (__stdcall *GCCC_CompatibilityCheck)(BOOL, DWORD *);
typedef BOOL (__stdcall *GCCC_LaunchGC)(HANDLE *);
typedef BOOL (__stdcall *GCCC_LaunchGCWithDimensions)(int, int, int, int);
typedef int (__stdcall *GCCC_GoogleChromeDaysSinceLastRun)();
}  // extern "C"

#endif  // CHROME_INSTALLER_GCAPI_GCAPI_H_
