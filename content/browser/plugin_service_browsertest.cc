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
#include "content/browser/resource_context.h"
#include "content/public/common/content_switches.h"
#include "content/test/test_browser_thread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "webkit/plugins/npapi/plugin_list.h"

using content::BrowserThread;

namespace {

const char kNPAPITestPluginMimeType[] = "application/vnd.npapi-test";

void OpenChannel(PluginProcessHost::Client* client) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  // Start opening the channel
  PluginService::GetInstance()->OpenChannelToNpapiPlugin(
      0, 0, GURL(), GURL(), kNPAPITestPluginMimeType, client);
}

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
  virtual void OnFoundPluginProcessHost(PluginProcessHost* host) OVERRIDE {}
  virtual void OnSentPluginChannelRequest() OVERRIDE {}

  virtual void OnChannelOpened(const IPC::ChannelHandle& handle) OVERRIDE {
    ASSERT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::IO));
    ASSERT_TRUE(set_plugin_info_called_);
    ASSERT_TRUE(!channel_);
    channel_ = new IPC::Channel(handle, IPC::Channel::MODE_CLIENT, this);
    ASSERT_TRUE(channel_->Connect());
  }

  void SetPluginInfo(const webkit::WebPluginInfo& info) OVERRIDE {
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
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&OpenChannel, &mock_client));
  ui_test_utils::RunMessageLoop();
}

// A strict mock that fails if any of the methods are called. They shouldn't be
// called since the request should get canceled before then.
class MockCanceledPluginServiceClient : public PluginProcessHost::Client {
 public:
  MockCanceledPluginServiceClient(const content::ResourceContext& context)
      : context_(context),
        get_resource_context_called_(false) {
  }

  virtual ~MockCanceledPluginServiceClient() {}

  // Client implementation.
  MOCK_METHOD0(ID, int());
  virtual const content::ResourceContext& GetResourceContext() OVERRIDE {
    get_resource_context_called_ = true;
    return context_;
  }
  MOCK_METHOD0(OffTheRecord, bool());
  MOCK_METHOD1(OnFoundPluginProcessHost, void(PluginProcessHost* host));
  MOCK_METHOD0(OnSentPluginChannelRequest, void());
  MOCK_METHOD1(OnChannelOpened, void(const IPC::ChannelHandle& handle));
  MOCK_METHOD1(SetPluginInfo, void(const webkit::WebPluginInfo& info));
  MOCK_METHOD0(OnError, void());

  bool get_resource_context_called() const {
    return get_resource_context_called_;
  }

 private:
  const content::ResourceContext& context_;
  bool get_resource_context_called_;

  DISALLOW_COPY_AND_ASSIGN(MockCanceledPluginServiceClient);
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
IN_PROC_BROWSER_TEST_F(PluginServiceTest, CancelOpenChannelToPluginService) {
  ::testing::StrictMock<MockCanceledPluginServiceClient> mock_client(
      browser()->profile()->GetResourceContext());
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                          base::Bind(OpenChannelAndThenCancel, &mock_client));
  ui_test_utils::RunMessageLoop();
  EXPECT_TRUE(mock_client.get_resource_context_called());
}

class MockCanceledBeforeSentPluginProcessHostClient
    : public MockCanceledPluginServiceClient {
 public:
  MockCanceledBeforeSentPluginProcessHostClient(
      const content::ResourceContext& context)
      : MockCanceledPluginServiceClient(context),
        set_plugin_info_called_(false),
        on_found_plugin_process_host_called_(false),
        host_(NULL) {}

  virtual ~MockCanceledBeforeSentPluginProcessHostClient() {}

  // Client implementation.
  virtual void SetPluginInfo(const webkit::WebPluginInfo& info) OVERRIDE {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    ASSERT_TRUE(info.mime_types.size());
    ASSERT_EQ(kNPAPITestPluginMimeType, info.mime_types[0].mime_type);
    set_plugin_info_called_ = true;
  }
  virtual void OnFoundPluginProcessHost(PluginProcessHost* host) OVERRIDE {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    set_on_found_plugin_process_host_called();
    set_host(host);
    // This gets called right before we request the plugin<=>renderer channel,
    // so we have to post a task to cancel it.
    MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&PluginProcessHost::CancelPendingRequest,
                   base::Unretained(host), this));
    MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&QuitUIMessageLoopFromIOThread));
  }

  bool set_plugin_info_called() const {
    return set_plugin_info_called_;
  }

  bool on_found_plugin_process_host_called() const {
    return on_found_plugin_process_host_called_;
  }

 protected:
  void set_on_found_plugin_process_host_called() {
    on_found_plugin_process_host_called_ = true;
  }
  void set_host(PluginProcessHost* host) {
    host_ = host;
  }

  PluginProcessHost* host() const { return host_; }

 private:
  bool set_plugin_info_called_;
  bool on_found_plugin_process_host_called_;
  PluginProcessHost* host_;

  DISALLOW_COPY_AND_ASSIGN(MockCanceledBeforeSentPluginProcessHostClient);
};

IN_PROC_BROWSER_TEST_F(
    PluginServiceTest, CancelBeforeSentOpenChannelToPluginProcessHost) {
  ::testing::StrictMock<MockCanceledBeforeSentPluginProcessHostClient>
      mock_client(browser()->profile()->GetResourceContext());
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&OpenChannel, &mock_client));
  ui_test_utils::RunMessageLoop();
  EXPECT_TRUE(mock_client.get_resource_context_called());
  EXPECT_TRUE(mock_client.set_plugin_info_called());
  EXPECT_TRUE(mock_client.on_found_plugin_process_host_called());
}

class MockCanceledAfterSentPluginProcessHostClient
    : public MockCanceledBeforeSentPluginProcessHostClient {
 public:
  MockCanceledAfterSentPluginProcessHostClient(
      const content::ResourceContext& context)
      : MockCanceledBeforeSentPluginProcessHostClient(context),
        on_sent_plugin_channel_request_called_(false) {}
  virtual ~MockCanceledAfterSentPluginProcessHostClient() {}

  // Client implementation.

  virtual int ID() OVERRIDE { return 42; }
  virtual bool OffTheRecord() OVERRIDE { return false; }

  // We override this guy again since we don't want to cancel yet.
  virtual void OnFoundPluginProcessHost(PluginProcessHost* host) OVERRIDE {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    set_on_found_plugin_process_host_called();
    set_host(host);
  }

  virtual void OnSentPluginChannelRequest() OVERRIDE {
    on_sent_plugin_channel_request_called_ = true;
    host()->CancelSentRequest(this);
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                            new MessageLoop::QuitTask());
  }

  bool on_sent_plugin_channel_request_called() const {
    return on_sent_plugin_channel_request_called_;
  }

 private:
  bool on_sent_plugin_channel_request_called_;

  DISALLOW_COPY_AND_ASSIGN(MockCanceledAfterSentPluginProcessHostClient);
};

// Should not attempt to open a channel, since it should be canceled early on.
IN_PROC_BROWSER_TEST_F(
    PluginServiceTest, CancelAfterSentOpenChannelToPluginProcessHost) {
  ::testing::StrictMock<MockCanceledAfterSentPluginProcessHostClient>
      mock_client(browser()->profile()->GetResourceContext());
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&OpenChannel, &mock_client));
  ui_test_utils::RunMessageLoop();
  EXPECT_TRUE(mock_client.get_resource_context_called());
  EXPECT_TRUE(mock_client.set_plugin_info_called());
  EXPECT_TRUE(mock_client.on_found_plugin_process_host_called());
  EXPECT_TRUE(mock_client.on_sent_plugin_channel_request_called());
}

}  // namespace
