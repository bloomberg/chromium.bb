// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_frame/test_utils.h"

#include <atlbase.h>
#include <atlwin.h>

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "chrome/common/chrome_paths.h"
#include "testing/gtest/include/gtest/gtest.h"

// Statics

FilePath ScopedChromeFrameRegistrar::GetChromeFrameBuildPath() {
  FilePath build_path;
  PathService::Get(chrome::DIR_APP, &build_path);
  build_path = build_path.Append(L"servers").
                          Append(L"npchrome_tab.dll");
  file_util::PathExists(build_path);
  return build_path;
}

void ScopedChromeFrameRegistrar::RegisterDefaults() {
  FilePath dll_path = GetChromeFrameBuildPath();
  RegisterAtPath(dll_path.value());
}

void ScopedChromeFrameRegistrar::RegisterAtPath(
    const std::wstring& path) {

  ASSERT_FALSE(path.empty());
  HMODULE chrome_frame_dll_handle = LoadLibrary(path.c_str());
  ASSERT_TRUE(chrome_frame_dll_handle != NULL);

  typedef HRESULT (STDAPICALLTYPE* DllRegisterServerFn)();
  DllRegisterServerFn register_server =
      reinterpret_cast<DllRegisterServerFn>(GetProcAddress(
          chrome_frame_dll_handle, "DllRegisterServer"));

  ASSERT_TRUE(register_server != NULL);
  EXPECT_HRESULT_SUCCEEDED((*register_server)());

  DllRegisterServerFn register_npapi_server =
      reinterpret_cast<DllRegisterServerFn>(GetProcAddress(
          chrome_frame_dll_handle, "RegisterNPAPIPlugin"));

  if (register_npapi_server != NULL)
    EXPECT_HRESULT_SUCCEEDED((*register_npapi_server)());

  ASSERT_TRUE(FreeLibrary(chrome_frame_dll_handle));
}

// Non-statics

ScopedChromeFrameRegistrar::ScopedChromeFrameRegistrar() {
  original_dll_path_ = GetChromeFrameBuildPath().ToWStringHack();
  RegisterChromeFrameAtPath(original_dll_path_);
}

ScopedChromeFrameRegistrar::~ScopedChromeFrameRegistrar() {
  if (FilePath(original_dll_path_) != FilePath(new_chrome_frame_dll_path_)) {
    RegisterChromeFrameAtPath(original_dll_path_);
  }
}

void ScopedChromeFrameRegistrar::RegisterChromeFrameAtPath(
    const std::wstring& path) {
  RegisterAtPath(path);
  new_chrome_frame_dll_path_ = path;
}

void ScopedChromeFrameRegistrar::RegisterReferenceChromeFrameBuild() {
  std::wstring reference_build_dir;
  ASSERT_TRUE(PathService::Get(chrome::DIR_APP, &reference_build_dir));

  file_util::UpOneDirectory(&reference_build_dir);
  file_util::UpOneDirectory(&reference_build_dir);

  file_util::AppendToPath(&reference_build_dir, L"chrome");
  file_util::AppendToPath(&reference_build_dir, L"tools");
  file_util::AppendToPath(&reference_build_dir, L"test");
  file_util::AppendToPath(&reference_build_dir, L"reference_build");
  file_util::AppendToPath(&reference_build_dir, L"chrome_frame");
  file_util::AppendToPath(&reference_build_dir, L"servers");
  file_util::AppendToPath(&reference_build_dir, L"npchrome_tab.dll");

  RegisterChromeFrameAtPath(reference_build_dir);
}

std::wstring ScopedChromeFrameRegistrar::GetChromeFrameDllPath() const {
  return new_chrome_frame_dll_path_;
}
