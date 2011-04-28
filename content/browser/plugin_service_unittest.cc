// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/plugin_service.h"

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


  virtual void SetUp() {
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

  DISALLOW_COPY_AND_ASSIGN(PluginServiceTest);
};

TEST_F(PluginServiceTest, GetUILocale) {
  // Check for a non-empty locale string.
  EXPECT_NE("", plugin_service_->GetUILocale());
}

}  // namespace
