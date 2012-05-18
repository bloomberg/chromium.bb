// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_GCAPI_MAC_GCAPI_H_
#define CHROME_INSTALLER_GCAPI_MAC_GCAPI_H_
#pragma once

// Error conditions for GoogleChromeCompatibilityCheck().
#define GCCC_ERROR_USERLEVELALREADYPRESENT       (1 << 0)
#define GCCC_ERROR_SYSTEMLEVELALREADYPRESENT     (1 << 1)
#define GCCC_ERROR_ACCESSDENIED                  (1 << 2)
#define GCCC_ERROR_OSNOTSUPPORTED                (1 << 3)
#define GCCC_ERROR_ALREADYOFFERED                (1 << 4)
#define GCCC_ERROR_INTEGRITYLEVEL                (1 << 5)

#ifdef __cplusplus
extern "C" {
#endif

// This function returns YES if Google Chrome should be offered.
// If the return is NO, |reasons| explains why.  If you don't care for the
// reason, you can pass NULL for |reasons|.
// |set_flag| indicates whether a flag should be set indicating that Chrome was
// offered within the last six months; if passed NO, this method will not
// set the flag even if Chrome can be offered.  If passed TRUE, this method
// will set the flag only if Chrome can be offered.
// |shell_mode| should be set to one of GCAPI_INVOKED_STANDARD_SHELL or
// GCAPI_INVOKED_UAC_ELEVATION depending on whether this method is invoked
// from an elevated or non-elevated process. TODO(thakis): This doesn't make
// sense on mac, change comment.
int GoogleChromeCompatibilityCheck(unsigned* reasons);

// This function launches Google Chrome after a successful install.
int LaunchGoogleChrome();

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // CHROME_INSTALLER_GCAPI_MAC_GCAPI_H_
