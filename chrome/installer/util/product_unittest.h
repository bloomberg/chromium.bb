// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_UTIL_PRODUCT_UNITTEST_H_
#define CHROME_INSTALLER_UTIL_PRODUCT_UNITTEST_H_
#pragma once

#include <windows.h>

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/scoped_temp_dir.h"
#include "base/win/registry.h"
#include "testing/gtest/include/gtest/gtest.h"

class TestWithTempDir : public testing::Test {
 protected:
  virtual void SetUp();
  virtual void TearDown();

  ScopedTempDir test_dir_;
};

class TestWithTempDirAndDeleteTempOverrideKeys : public TestWithTempDir {
 protected:
  virtual void SetUp();
  virtual void TearDown();
};

// TODO(tommi): This is "borrowed" from Chrome Frame test code.  It should be
// moved to some common test utility file.
class TempRegKeyOverride {
 public:
  static const wchar_t kTempTestKeyPath[];

  TempRegKeyOverride(HKEY override, const wchar_t* temp_name);
  ~TempRegKeyOverride();

  static void DeleteAllTempKeys();

 protected:
  HKEY override_;
  base::win::RegKey temp_key_;
  std::wstring temp_name_;
};

#endif  // CHROME_INSTALLER_UTIL_PRODUCT_UNITTEST_H_
