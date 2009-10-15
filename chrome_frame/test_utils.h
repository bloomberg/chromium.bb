// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_FRAME_TEST_UTILS_H_
#define CHROME_FRAME_TEST_UTILS_H_

#include <string>

#include "base/file_path.h"

// Helper class used to register different chrome frame DLLs while running
// tests. At construction, this registers the DLL found in the build path.
// At destruction, again registers the DLL found in the build path if another
// DLL has since been registered. Triggers GTEST asserts on failure.
//
// TODO(robertshield): Ideally, make this class restore the originally
// registered chrome frame DLL (e.g. by looking in HKCR) on destruction.
class ScopedChromeFrameRegistrar {
 public:
  ScopedChromeFrameRegistrar();
  virtual ~ScopedChromeFrameRegistrar();

  void RegisterChromeFrameAtPath(const std::wstring& path);
  void RegisterReferenceChromeFrameBuild();

  std::wstring GetChromeFrameDllPath() const;

  static FilePath GetChromeFrameBuildPath();
  static void RegisterAtPath(const std::wstring& path);
  static void RegisterDefaults();

 private:
  // Contains the path of the most recently registered npchrome_tab.dll.
  std::wstring new_chrome_frame_dll_path_;

  // Contains the path of the npchrome_tab.dll to be registered at destruction.
  std::wstring original_dll_path_;
};

#endif  // CHROME_FRAME_TEST_UTILS_H_
