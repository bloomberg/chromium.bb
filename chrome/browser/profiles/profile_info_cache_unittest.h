// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_PROFILE_INFO_CACHE_UNITTEST_H_
#define CHROME_BROWSER_PROFILES_PROFILE_INFO_CACHE_UNITTEST_H_
#pragma once

#include "chrome/test/base/testing_profile_manager.h"
#include "content/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

class FilePath;
class ProfileInfoCache;

class ProfileInfoCacheTest : public testing::Test {
 protected:
  ProfileInfoCacheTest();
  virtual ~ProfileInfoCacheTest();

  virtual void SetUp() OVERRIDE;
  virtual void TearDown() OVERRIDE;

  ProfileInfoCache* GetCache();
  FilePath GetProfilePath(const std::string& base_name);
  void ResetCache();

 protected:
  TestingProfileManager testing_profile_manager_;

 private:
  MessageLoopForUI ui_loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread file_thread_;
};

#endif  // CHROME_BROWSER_PROFILES_PROFILE_INFO_CACHE_UNITTEST_H_
