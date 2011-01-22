// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/plugin_service.h"

#include "base/auto_reset.h"
#include "base/command_line.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/test/in_process_browser_test.h"
#include "chrome/test/testing_profile.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "webkit/plugins/npapi/plugin_list.h"

namespace {

// We have to mock the Client class up in order to be able to test the
// OpenChannelToPlugin function. The only really needed function of this mockup
// is SetPluginInfo, which gets called in
// PluginService::FinishOpenChannelToPlugin.
class MockPluginProcessHostClient : public PluginProcessHost::Client {
 public:
  MockPluginProcessHostClient() {}
  virtual ~MockPluginProcessHostClient() {}

  MOCK_METHOD0(ID, int());
  MOCK_METHOD0(OffTheRecord, bool());
  MOCK_METHOD1(SetPluginInfo, void(const webkit::npapi::WebPluginInfo& info));
  MOCK_METHOD1(OnChannelOpened, void(const IPC::ChannelHandle& handle));
  MOCK_METHOD0(OnError, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockPluginProcessHostClient);
};

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

// These tests need to be implemented as in process tests because on mac os the
// plugin loading mechanism checks whether plugin paths are in the bundle path
// and the test fails this check when run outside of the browser process.
IN_PROC_BROWSER_TEST_F(PluginServiceTest, StartAndFindPluginProcess) {
  // Try to load the default plugin and if this is successful consecutive
  // calls to FindPluginProcess should return non-zero values.
  PluginProcessHost* default_plugin_process_host =
      plugin_service_->FindOrStartPluginProcess(
          FilePath(webkit::npapi::kDefaultPluginLibraryName));

  EXPECT_EQ(default_plugin_process_host, plugin_service_->FindPluginProcess(
      FilePath(webkit::npapi::kDefaultPluginLibraryName)));
}

IN_PROC_BROWSER_TEST_F(PluginServiceTest, OpenChannelToPlugin) {
  MockPluginProcessHostClient mock_client;
  EXPECT_CALL(mock_client, SetPluginInfo(testing::_)).Times(1);
  plugin_service_->OpenChannelToPlugin(0, 0, GURL("http://google.com/"),
                                       "audio/mp3",
                                       &mock_client);
  message_loop_.RunAllPending();
}

IN_PROC_BROWSER_TEST_F(PluginServiceTest, GetFirstAllowedPluginInfo) {
  // on ChromeOS the plugin policy gets loaded on the FILE thread and the
  // GetFirstAllowedPluginInfo will fail if we don't allow it to finish.
  message_loop_.RunAllPending();
  // We should always get a positive response no matter whether we really have
  // a plugin to support that particular mime type because the Default plugin
  // supports all mime types.
  webkit::npapi::WebPluginInfo plugin_info;
  std::string plugin_mime_type;
  plugin_service_->GetFirstAllowedPluginInfo(0, 0, GURL("http://google.com/"),
                                             "application/pdf",
                                             &plugin_info,
                                             &plugin_mime_type);
  EXPECT_EQ("application/pdf", plugin_mime_type);
}

}  // namespace
