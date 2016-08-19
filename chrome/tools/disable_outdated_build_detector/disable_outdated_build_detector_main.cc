// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The main entrypoint for the tool that disables the outdated build detector
// for organic installs of Chrome on Windows XP and Vista. See
// disable_outdated_build_detector.h for more information.

#include <windows.h>

#include "chrome/tools/disable_outdated_build_detector/disable_outdated_build_detector.h"

namespace {

bool IsVistaOrOlder() {
  OSVERSIONINFO version_info = {sizeof(version_info)};
  return ::GetVersionEx(&version_info) && (version_info.dwMajorVersion < 6 ||
                                           (version_info.dwMajorVersion == 6 &&
                                            version_info.dwMinorVersion == 0));
}

}  // namespace

int WINAPI wWinMain(HINSTANCE /* instance */,
                    HINSTANCE /* unused */,
                    wchar_t* command_line,
                    int /* command_show */) {
  if (!IsVistaOrOlder())
    return static_cast<int>(ExitCode::UNSUPPORTED_OS);

  // For reasons, wWinMain returns an int rather than a DWORD. This value
  // eventually makes its way to ExitProcess, which indeed takes an unsigned
  // int.
  return static_cast<int>(DisableOutdatedBuildDetector(command_line));
}
