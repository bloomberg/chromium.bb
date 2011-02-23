// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/plugin_service.h"

#include "base/auto_reset.h"
#include "base/command_line.h"
#include "chrome/test/testing_profile.h"
#include "content/browser/browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class PluginServiceTest : public testing::Test {
 public:
  PluginServiceTest()
      : message_loop_(MessageLoop::TYPE_IO),
        ui_thread_(BrowserThread::UI, &message_loop_),
        file_thread_(BrowserThread::FILE, &message_loop_),
        io_thread_(BrowserThread::IO, &message_loop_) {}

  virtual ~PluginServiceTest() {}

  virtual void SetUp() {
    profile_.reset(new TestingProfile());

    PluginService::InitGlobalInstance(profile_.get());
    plugin_service_ = PluginService::GetInstance();
    ASSERT_TRUE(plugin_service_);
  }

 protected:
  MessageLoop message_loop_;
  PluginService* plugin_service_;

 private:
  BrowserThread ui_thread_;
  BrowserThread file_thread_;
  BrowserThread io_thread_;
  scoped_ptr<TestingProfile> profile_;

  DISALLOW_COPY_AND_ASSIGN(PluginServiceTest);
};

TEST_F(PluginServiceTest, SetGetChromePluginDataDir) {
  // Check that after setting the same plugin dir we just read it is set
  // correctly.
  FilePath plugin_data_dir = plugin_service_->GetChromePluginDataDir();
  FilePath new_plugin_data_dir(FILE_PATH_LITERAL("/a/bogus/dir"));
  plugin_service_->SetChromePluginDataDir(new_plugin_data_dir);
  EXPECT_EQ(new_plugin_data_dir, plugin_service_->GetChromePluginDataDir());
  plugin_service_->SetChromePluginDataDir(plugin_data_dir);
  EXPECT_EQ(plugin_data_dir, plugin_service_->GetChromePluginDataDir());
}

TEST_F(PluginServiceTest, GetUILocale) {
  // Check for a non-empty locale string.
  EXPECT_NE("", plugin_service_->GetUILocale());
}

}  // namespace
