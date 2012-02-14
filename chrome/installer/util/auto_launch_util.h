// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_UTIL_AUTO_LAUNCH_UTIL_H_
#define CHROME_INSTALLER_UTIL_AUTO_LAUNCH_UTIL_H_
#pragma once

class FilePath;

// A namespace containing the platform specific implementation of setting Chrome
// to launch on computer startup.
namespace auto_launch_util {

// Returns whether the Chrome executable specified in |application_path| is set
// to auto-launch at computer startup. NOTE: |application_path| is optional and
// should be blank in most cases (as it will default to the application path of
// the current executable).
bool WillLaunchAtLogin(const FilePath& application_path);

// Configures whether the Chrome executable should auto-launch at computer
// startup. |application_path| is needed when |auto_launch| is true AND when
// the caller is not the process being set to auto-launch, ie. the installer.
// This is because |application_path|, if left blank, defaults to the
// application path of the current executable. If |auto_launch| is true, then it
// will auto-launch, otherwise it will be configured not to.
void SetWillLaunchAtLogin(bool auto_launch, const FilePath& application_path);

}  // namespace auto_launch_util

#endif  // CHROME_INSTALLER_UTIL_AUTO_LAUNCH_UTIL_H_
