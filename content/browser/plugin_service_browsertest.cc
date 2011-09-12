// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/plugin_service.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/path_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/browser/browser_thread.h"
#include "content/browser/resource_context.h"
#include "content/common/content_switches.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "webkit/plugins/npapi/plugin_list.h"

namespace {

const char* kNPAPITestPluginMimeType = "application/vnd.npapi-test";

// Mock up of the Client and the Listener classes that would supply the
// communication channel with the plugin.
class MockPluginProcessHostClient : public PluginProcessHost::Client,
                                    public IPC::Channel::Listener {
 public:
  MockPluginProcessHostClient(const content::ResourceContext& context)
      : context_(context),
        channel_(NULL),
        set_plugin_info_called_(false) {
  }

  virtual ~MockPluginProcessHostClient() {
    if (channel_)
      BrowserThread::DeleteSoon(BrowserThread::IO, FROM_HERE, channel_);
  }

  // Client implementation.
  virtual int ID() OVERRIDE { return 42; }
  virtual bool OffTheRecord() OVERRIDE { return false; }
  virtual const content::ResourceContext& GetResourceContext() OVERRIDE {
    return context_;
  }
  virtual void OnFoundPluginProcessHost(PluginProcessHost* host) {
  }

  void OnChannelOpened(const IPC::ChannelHandle& handle) {
    ASSERT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::IO));
    ASSERT_TRUE(set_plugin_info_called_);
    ASSERT_TRUE(!channel_);
    channel_ = new IPC::Channel(handle, IPC::Channel::MODE_CLIENT, this);
    ASSERT_TRUE(channel_->Connect());
  }

  void SetPluginInfo(const webkit::WebPluginInfo& info) {
    ASSERT_TRUE(info.mime_types.size());
    ASSERT_EQ(kNPAPITestPluginMimeType, info.mime_types[0].mime_type);
    set_plugin_info_called_ = true;
  }

  MOCK_METHOD0(OnError, void());

  // Listener implementation.
  MOCK_METHOD1(OnMessageReceived, bool(const IPC::Message& message));
  void OnChannelConnected(int32 peer_pid) OVERRIDE {
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                            new MessageLoop::QuitTask());
  }
  MOCK_METHOD0(OnChannelError, void());
  MOCK_METHOD0(OnChannelDenied, void());
  MOCK_METHOD0(OnChannelListenError, void());

 private:
  const content::ResourceContext& context_;
  IPC::Channel* channel_;
  bool set_plugin_info_called_;
  DISALLOW_COPY_AND_ASSIGN(MockPluginProcessHostClient);
};

class PluginServiceTest : public InProcessBrowserTest {
 public:
  PluginServiceTest() : InProcessBrowserTest() { }

  virtual void SetUpCommandLine(CommandLine* command_line) {
#ifdef OS_MACOSX
    FilePath browser_directory;
    PathService::Get(base::DIR_MODULE, &browser_directory);
    command_line->AppendSwitchPath(switches::kExtraPluginDir,
                                   browser_directory.AppendASCII("plugins"));
#endif
  }
};

// Try to open a channel to the test plugin. Minimal plugin process spawning
// test for the PluginService interface.
IN_PROC_BROWSER_TEST_F(PluginServiceTest, OpenChannelToPlugin) {
  ::testing::StrictMock<MockPluginProcessHostClient> mock_client(
      browser()->profile()->GetResourceContext());
  PluginService::GetInstance()->OpenChannelToNpapiPlugin(
      0, 0, GURL(), GURL(), kNPAPITestPluginMimeType, &mock_client);
  ui_test_utils::RunMessageLoop();
}

// A strict mock that fails if any of the methods are called. They shouldn't be
// called since the request should get canceled before then.
class MockCanceledPluginProcessHostClient: public PluginProcessHost::Client {
 public:
  // Client implementation.
  MOCK_METHOD1(OnFoundPluginProcessHost, void(PluginProcessHost* host));
  MOCK_METHOD1(OnChannelOpened, void(const IPC::ChannelHandle& handle));
  MOCK_METHOD1(SetPluginInfo, void(const webkit::WebPluginInfo& info));
  MOCK_METHOD0(OnError, void());

  // Listener implementation.
  MOCK_METHOD1(OnMessageReceived, bool(const IPC::Message& message));
  MOCK_METHOD1(OnChannelConnected, void(int32 peer_pid));
  MOCK_METHOD0(OnChannelError, void());
  MOCK_METHOD0(OnChannelDenied, void());
  MOCK_METHOD0(OnChannelListenError, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockCanceledPluginProcessHostClient);
};

void DoNothing() {}

void QuitUIMessageLoopFromIOThread() {
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                          new MessageLoop::QuitTask());
}

void OpenChannelAndThenCancel(PluginProcessHost::Client* client) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  // Start opening the channel
  PluginService::GetInstance()->OpenChannelToNpapiPlugin(
      0, 0, GURL(), GURL(), kNPAPITestPluginMimeType, client);
  // Immediately cancel it. This is guaranteed to work since PluginService needs
  // to consult its filter on the FILE thread.
  PluginService::GetInstance()->CancelOpenChannelToNpapiPlugin(client);
  // Before we terminate the test, add a roundtrip through the FILE thread to
  // make sure that it's had a chance to post back to the IO thread. Then signal
  // the UI thread to stop and exit the test.
  BrowserThread::PostTaskAndReply(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&DoNothing),
      base::Bind(&QuitUIMessageLoopFromIOThread));
}

// Should not attempt to open a channel, since it should be canceled early on.
IN_PROC_BROWSER_TEST_F(PluginServiceTest, CancelOpenChannelToPlugin) {
  ::testing::StrictMock<MockPluginProcessHostClient> mock_client(
      browser()->profile()->GetResourceContext());
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      NewRunnableFunction(OpenChannelAndThenCancel, &mock_client));
  ui_test_utils::RunMessageLoop();
}

}  // namespace
