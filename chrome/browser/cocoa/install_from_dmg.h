// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COCOA_INSTALL_FROM_DMG_H_
#define CHROME_BROWSER_COCOA_INSTALL_FROM_DMG_H_
#pragma once

// If the application is running from a read-only disk image, prompts the user
// to install it to the hard drive.  If the user approves, the application
// will be installed and launched, and MaybeInstallFromDiskImage will return
// true.  In that case, the caller must exit expeditiously.
bool MaybeInstallFromDiskImage();

#endif  // CHROME_BROWSER_COCOA_INSTALL_FROM_DMG_H_
