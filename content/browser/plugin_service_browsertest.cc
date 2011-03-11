// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/plugin_service.h"

#include "base/command_line.h"
#include "base/path_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/in_process_browser_test.h"
#include "chrome/test/ui_test_utils.h"
#include "content/browser/browser_thread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "webkit/plugins/npapi/plugin_list.h"

namespace {

const char* kNPAPITestPluginMimeType = "application/vnd.npapi-test";

// Mock up of the Client and the Listener classes that would supply the
// communication channel with the plugin.
class MockPluginProcessHostClient : public PluginProcessHost::Client,
                                    public IPC::Channel::Listener {
 public:
  MockPluginProcessHostClient()
      : channel_(NULL),
        set_plugin_info_called_(false) {
  }

  virtual ~MockPluginProcessHostClient() {
    if (channel_)
      BrowserThread::DeleteSoon(BrowserThread::IO, FROM_HERE, channel_);
  }

  // Client implementation.
  int ID() { return 42; }
  bool OffTheRecord() { return false; }

  void OnChannelOpened(const IPC::ChannelHandle& handle) {
    ASSERT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::IO));
    ASSERT_TRUE(set_plugin_info_called_);
    ASSERT_TRUE(!channel_);
    channel_ = new IPC::Channel(handle, IPC::Channel::MODE_CLIENT, this);
    ASSERT_TRUE(channel_->Connect());
  }

  void SetPluginInfo(const webkit::npapi::WebPluginInfo& info) {
    ASSERT_TRUE(info.mime_types.size());
    ASSERT_EQ(kNPAPITestPluginMimeType, info.mime_types[0].mime_type);
    set_plugin_info_called_ = true;
  }

  MOCK_METHOD0(OnError, void());

  // Listener implementation.
  MOCK_METHOD1(OnMessageReceived, bool(const IPC::Message& message));
  void OnChannelConnected(int32 peer_pid) {
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                            new MessageLoop::QuitTask());
  }
  MOCK_METHOD0(OnChannelError, void());
  MOCK_METHOD0(OnChannelDenied, void());
  MOCK_METHOD0(OnChannelListenError, void());

 private:
  IPC::Channel* channel_;
  bool set_plugin_info_called_;
  DISALLOW_COPY_AND_ASSIGN(MockPluginProcessHostClient);
};

}  // namespace

class PluginServiceTest : public InProcessBrowserTest {
 public:
  PluginServiceTest() : InProcessBrowserTest() { }

  virtual void SetUpCommandLine(CommandLine* command_line) {
#ifdef OS_MACOSX
    FilePath browser_directory;
    PathService::Get(chrome::DIR_APP, &browser_directory);
    command_line->AppendSwitchPath(switches::kExtraPluginDir,
                                   browser_directory.AppendASCII("plugins"));
#endif
  }
};

// Try to open a channel to the test plugin. Minimal plugin process spawning
// test for the PluginService interface.
IN_PROC_BROWSER_TEST_F(PluginServiceTest, OpenChannelToPlugin) {
  MockPluginProcessHostClient mock_client;
  PluginService::GetInstance()->OpenChannelToNpapiPlugin(
      0, 0, GURL(), kNPAPITestPluginMimeType, &mock_client);
  ui_test_utils::RunMessageLoop();
}
