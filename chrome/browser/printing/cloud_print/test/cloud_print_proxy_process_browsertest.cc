// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Create a service process that uses a Mock to respond to the browser in order
// to test launching the browser using the cloud print policy check command
// line switch.

#include "base/bind.h"
#include "base/command_line.h"
#include "base/message_loop.h"
#include "base/process_util.h"
#include "base/rand_util.h"
#include "base/test/multiprocess_test.h"
#include "base/test/test_timeouts.h"
#include "base/time.h"
#include "chrome/browser/printing/cloud_print/cloud_print_proxy_service.h"
#include "chrome/browser/printing/cloud_print/cloud_print_proxy_service_factory.h"
#include "chrome/browser/profiles/profile_keyed_service.h"
#include "chrome/browser/service/service_process_control.h"
#include "chrome/browser/ui/browser_init.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/service_messages.h"
#include "chrome/common/service_process_util.h"
#include "chrome/service/service_ipc_server.h"
#include "chrome/service/service_process.h"
#include "chrome/test/base/test_launcher_utils.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_pref_service.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/notification_service.h"
#include "content/test/test_browser_thread.h"
#include "ipc/ipc_descriptors.h"
#include "ipc/ipc_switches.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/multiprocess_func_list.h"

#if defined(OS_MACOSX)
#include "chrome/common/mac/mock_launchd.h"
#endif
#if defined(OS_POSIX)
#include "base/global_descriptors_posix.h"
#endif

using ::testing::AnyNumber;
using ::testing::Assign;
using ::testing::AtLeast;
using ::testing::DoAll;
using ::testing::Invoke;
using ::testing::Mock;
using ::testing::Property;
using ::testing::Return;
using ::testing::WithoutArgs;
using ::testing::_;

namespace {

enum MockServiceProcessExitCodes {
  kMissingSwitch = 1,
  kInitializationFailure,
  kExpectationsNotMet,
  kShutdownNotGood
};

#if defined(OS_MACOSX)
const char kTestExecutablePath[] = "test-executable-path";
#endif

bool g_good_shutdown = false;

void ShutdownTask() {
  g_good_shutdown = true;
  g_service_process->Shutdown();
}

class TestStartupClientChannelListener : public IPC::Channel::Listener {
 public:
  virtual bool OnMessageReceived(const IPC::Message& message) { return false; }
};

}  // namespace

class TestServiceProcess : public ServiceProcess {
 public:
  TestServiceProcess() { }
  virtual ~TestServiceProcess() { }

  bool Initialize(MessageLoopForUI* message_loop, ServiceProcessState* state);

  base::MessageLoopProxy* IOMessageLoopProxy() {
    return io_thread_->message_loop_proxy();
  }
};

bool TestServiceProcess::Initialize(MessageLoopForUI* message_loop,
                                    ServiceProcessState* state) {
  main_message_loop_ = message_loop;

  service_process_state_.reset(state);

  base::Thread::Options options(MessageLoop::TYPE_IO, 0);
  io_thread_.reset(new base::Thread("TestServiceProcess_IO"));
  return io_thread_->StartWithOptions(options);
}

// This mocks the service side IPC message handler, allowing us to have a
// minimal service process.
class MockServiceIPCServer : public ServiceIPCServer {
 public:
  static std::string EnabledUserId();

  explicit MockServiceIPCServer(const IPC::ChannelHandle& handle)
      : ServiceIPCServer(handle),
        enabled_(true) { }

  MOCK_METHOD1(OnMessageReceived, bool(const IPC::Message& message));
  MOCK_METHOD1(OnChannelConnected, void(int32 peer_pid));
  MOCK_METHOD0(OnChannelError, void());

  void SetServiceEnabledExpectations();
  void SetWillBeDisabledExpectations();

  void CallServiceOnChannelConnected(int32 peer_pid) {
    ServiceIPCServer::OnChannelConnected(peer_pid);
  }

  bool SendInfo();

 private:
  cloud_print::CloudPrintProxyInfo info_;
  bool enabled_;
};

// static
std::string MockServiceIPCServer::EnabledUserId() {
  return std::string("kitteh@canhazcheezburger.cat");
}

void MockServiceIPCServer::SetServiceEnabledExpectations() {
  EXPECT_CALL(*this, OnChannelConnected(_)).Times(1)
      .WillRepeatedly(
          Invoke(this, &MockServiceIPCServer::CallServiceOnChannelConnected));

  EXPECT_CALL(*this, OnChannelError()).Times(0);
  EXPECT_CALL(*this, OnMessageReceived(_)).Times(0);

  EXPECT_CALL(*this,
      OnMessageReceived(
          Property(&IPC::Message::type,
                   static_cast<int32>(ServiceMsg_GetCloudPrintProxyInfo::ID))))
      .Times(AnyNumber()).WillRepeatedly(
          WithoutArgs(Invoke(this, &MockServiceIPCServer::SendInfo)));

  EXPECT_CALL(*this,
              OnMessageReceived(
                  Property(&IPC::Message::type,
                           static_cast<int32>(ServiceMsg_Shutdown::ID))))
      .Times(1)
      .WillOnce(
          DoAll(Assign(&g_good_shutdown, true),
                WithoutArgs(
                    Invoke(g_service_process, &ServiceProcess::Shutdown)),
                Return(true)));
}

void MockServiceIPCServer::SetWillBeDisabledExpectations() {
  SetServiceEnabledExpectations();

  EXPECT_CALL(*this,
              OnMessageReceived(
                  Property(&IPC::Message::type,
                           static_cast<int32>(
                               ServiceMsg_DisableCloudPrintProxy::ID))))
      .Times(AtLeast(1))
      .WillRepeatedly(DoAll(Assign(&enabled_, false), Return(true)));
}

bool MockServiceIPCServer::SendInfo() {
  if (enabled_) {
    info_.enabled = true;
    info_.email = EnabledUserId();
    EXPECT_TRUE(Send(new ServiceHostMsg_CloudPrintProxy_Info(info_)));
  } else {
    info_.enabled = false;
    info_.email = std::string();
    EXPECT_TRUE(Send(new ServiceHostMsg_CloudPrintProxy_Info(info_)));
  }
  return true;
}

typedef base::Callback<void(MockServiceIPCServer* server)>
    SetExpectationsCallback;

// The return value from this routine is used as the exit code for the mock
// service process. Any non-zero return value will be printed out and can help
// determine the failure.
int CloudPrintMockService_Main(SetExpectationsCallback set_expectations) {
  MessageLoopForUI main_message_loop;
  main_message_loop.set_thread_name("Main Thread");

#if defined(OS_MACOSX)
  CommandLine* cl = CommandLine::ForCurrentProcess();
  if (!cl->HasSwitch(kTestExecutablePath))
    return kMissingSwitch;
  FilePath executable_path = cl->GetSwitchValuePath(kTestExecutablePath);
  EXPECT_FALSE(executable_path.empty());
  MockLaunchd mock_launchd(executable_path, &main_message_loop, true, true);
  Launchd::ScopedInstance use_mock(&mock_launchd);
#endif

  ServiceProcessState* state(new ServiceProcessState);
  bool service_process_state_initialized = state->Initialize();
  EXPECT_TRUE(service_process_state_initialized);
  if (!service_process_state_initialized)
    return kInitializationFailure;

  TestServiceProcess service_process;
  EXPECT_EQ(&service_process, g_service_process);

  // Takes ownership of the pointer, but we can use it since we have the same
  // lifetime.
  EXPECT_TRUE(service_process.Initialize(&main_message_loop, state));

  MockServiceIPCServer server(state->GetServiceProcessChannel());

  // Here is where the expectations/mock responses need to be set up.
  set_expectations.Run(&server);

  EXPECT_TRUE(server.Init());
  EXPECT_TRUE(state->SignalReady(service_process.IOMessageLoopProxy(),
                                 base::Bind(&ShutdownTask)));
#if defined(OS_MACOSX)
  mock_launchd.SignalReady();
#endif

  // Connect up the parent/child IPC channel to signal that the test can
  // continue.
  TestStartupClientChannelListener listener;
  EXPECT_TRUE(CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kProcessChannelID));
  std::string startup_channel_name =
      CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kProcessChannelID);
  scoped_ptr<IPC::ChannelProxy> startup_channel;
  startup_channel.reset(
      new IPC::ChannelProxy(startup_channel_name,
                            IPC::Channel::MODE_CLIENT,
                            &listener,
                            service_process.IOMessageLoopProxy()));

  main_message_loop.Run();
  if (!Mock::VerifyAndClearExpectations(&server))
    return kExpectationsNotMet;
  if (!g_good_shutdown)
    return kShutdownNotGood;
  return 0;
}

void SetServiceEnabledExpectations(MockServiceIPCServer* server) {
  server->SetServiceEnabledExpectations();
}

MULTIPROCESS_TEST_MAIN(CloudPrintMockService_StartEnabledWaitForQuit) {
  return CloudPrintMockService_Main(
      base::Bind(&SetServiceEnabledExpectations));
}

void SetServiceWillBeDisabledExpectations(MockServiceIPCServer* server) {
  server->SetWillBeDisabledExpectations();
}

MULTIPROCESS_TEST_MAIN(CloudPrintMockService_StartEnabledExpectDisabled) {
  return CloudPrintMockService_Main(
      base::Bind(&SetServiceWillBeDisabledExpectations));
}

class CloudPrintProxyPolicyStartupTest : public base::MultiProcessTest,
                                         public IPC::Channel::Listener {
 public:
  CloudPrintProxyPolicyStartupTest();
  ~CloudPrintProxyPolicyStartupTest();

  virtual void SetUp();
  base::MessageLoopProxy* IOMessageLoopProxy() {
    return io_thread_.message_loop_proxy();
  }
  base::ProcessHandle Launch(const std::string& name);
  void WaitForConnect();
  bool Send(IPC::Message* message);
  void ShutdownAndWaitForExitWithTimeout(base::ProcessHandle handle);

  // IPC::Channel::Listener implementation
  virtual bool OnMessageReceived(const IPC::Message& message) { return false; }
  virtual void OnChannelConnected(int32 peer_pid);

  // MultiProcessTest implementation.
  virtual CommandLine MakeCmdLine(const std::string& procname,
                                  bool debug_on_start) OVERRIDE;

  bool LaunchBrowser(const CommandLine& command_line, Profile* profile) {
    int return_code = 0;
    BrowserInit browser_init;
    return BrowserInit::ProcessCmdLineImpl(command_line, FilePath(), false,
                                           profile, BrowserInit::Profiles(),
                                           &return_code, &browser_init);
  }

 protected:
  MessageLoopForUI message_loop_;
  content::TestBrowserThread ui_thread_;
  base::Thread io_thread_;

  std::string startup_channel_id_;
  scoped_ptr<IPC::ChannelProxy> startup_channel_;

#if defined(OS_MACOSX)
  ScopedTempDir temp_dir_;
  FilePath executable_path_, bundle_path_;
  scoped_ptr<MockLaunchd> mock_launchd_;
  scoped_ptr<Launchd::ScopedInstance> scoped_launchd_instance_;
#endif

 private:
  class WindowedChannelConnectionObserver {
   public:
    WindowedChannelConnectionObserver()
        : seen_(false),
          running_(false) { }

    void Wait() {
      if (seen_)
        return;
      running_ = true;
      ui_test_utils::RunMessageLoop();
    }

    void Notify() {
      seen_ = true;
      if (running_)
        MessageLoopForUI::current()->Quit();
    }

   private:
    bool seen_;
    bool running_;
  };

  WindowedChannelConnectionObserver observer_;
};

CloudPrintProxyPolicyStartupTest::CloudPrintProxyPolicyStartupTest()
    : ui_thread_(content::BrowserThread::UI, &message_loop_),
      io_thread_("CloudPrintProxyPolicyTestThread") {
}

CloudPrintProxyPolicyStartupTest::~CloudPrintProxyPolicyStartupTest() {
}

void CloudPrintProxyPolicyStartupTest::SetUp() {
  base::Thread::Options options(MessageLoop::TYPE_IO, 0);
  ASSERT_TRUE(io_thread_.StartWithOptions(options));

#if defined(OS_MACOSX)
  EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());
  EXPECT_TRUE(MockLaunchd::MakeABundle(temp_dir_.path(),
                                       "CloudPrintProxyTest",
                                       &bundle_path_,
                                       &executable_path_));
  mock_launchd_.reset(new MockLaunchd(executable_path_, &message_loop_,
                                      true, false));
  scoped_launchd_instance_.reset(
      new Launchd::ScopedInstance(mock_launchd_.get()));
#endif
}

base::ProcessHandle CloudPrintProxyPolicyStartupTest::Launch(
    const std::string& name) {
  EXPECT_FALSE(CheckServiceProcessReady());

  startup_channel_id_ =
      base::StringPrintf("%d.%p.%d",
                         base::GetCurrentProcId(), this,
                         base::RandInt(0, std::numeric_limits<int>::max()));
  startup_channel_.reset(new IPC::ChannelProxy(
      startup_channel_id_, IPC::Channel::MODE_SERVER,
      this, IOMessageLoopProxy()));

#if defined(OS_POSIX)
  base::FileHandleMappingVector ipc_file_list;
  ipc_file_list.push_back(std::make_pair(
      startup_channel_->TakeClientFileDescriptor(),
      kPrimaryIPCChannel + base::GlobalDescriptors::kBaseDescriptor));
  base::ProcessHandle handle = SpawnChild(name, ipc_file_list, false);
#else
  base::ProcessHandle handle = SpawnChild(name, false);
#endif
  EXPECT_TRUE(handle);
  return handle;
}

void CloudPrintProxyPolicyStartupTest::WaitForConnect() {
  observer_.Wait();
  EXPECT_TRUE(CheckServiceProcessReady());
  EXPECT_TRUE(base::MessageLoopProxy::current());
  ServiceProcessControl::GetInstance()->SetChannel(new IPC::ChannelProxy(
      GetServiceProcessChannel(), IPC::Channel::MODE_NAMED_CLIENT,
      ServiceProcessControl::GetInstance(), IOMessageLoopProxy()));
}

bool CloudPrintProxyPolicyStartupTest::Send(IPC::Message* message) {
  return ServiceProcessControl::GetInstance()->Send(message);
}

void CloudPrintProxyPolicyStartupTest::ShutdownAndWaitForExitWithTimeout(
    base::ProcessHandle handle) {
  ASSERT_TRUE(Send(new ServiceMsg_Shutdown()));

  int exit_code = -100;
  bool exited =
      base::WaitForExitCodeWithTimeout(handle, &exit_code,
                                       TestTimeouts::action_timeout_ms());
  EXPECT_TRUE(exited);
  EXPECT_EQ(exit_code, 0);
  base::CloseProcessHandle(handle);
}

void CloudPrintProxyPolicyStartupTest::OnChannelConnected(int32 peer_pid) {
  observer_.Notify();
}

CommandLine CloudPrintProxyPolicyStartupTest::MakeCmdLine(
    const std::string& procname,
    bool debug_on_start) {
  CommandLine cl = MultiProcessTest::MakeCmdLine(procname, debug_on_start);
  cl.AppendSwitchASCII(switches::kProcessChannelID, startup_channel_id_);
#if defined(OS_MACOSX)
  cl.AppendSwitchASCII(kTestExecutablePath, executable_path_.value());
#endif
  return cl;
}

TEST_F(CloudPrintProxyPolicyStartupTest, StartAndShutdown) {
  base::ProcessHandle handle =
      Launch("CloudPrintMockService_StartEnabledWaitForQuit");
  WaitForConnect();
  ShutdownAndWaitForExitWithTimeout(handle);
}

ProfileKeyedService* CloudPrintProxyServiceFactoryForPolicyTest(
    Profile* profile) {
  CloudPrintProxyService* service = new CloudPrintProxyService(profile);
  service->Initialize();
  return service;
}

TEST_F(CloudPrintProxyPolicyStartupTest, StartBrowserWithoutPolicy) {
  base::ProcessHandle handle =
      Launch("CloudPrintMockService_StartEnabledWaitForQuit");

  TestingBrowserProcess* browser_process =
      static_cast<TestingBrowserProcess*>(g_browser_process);
  TestingProfileManager profile_manager(browser_process);
  ASSERT_TRUE(profile_manager.SetUp());

  TestingProfile* profile =
      profile_manager.CreateTestingProfile("StartBrowserWithoutPolicy");
  CloudPrintProxyServiceFactory::GetInstance()->
      SetTestingFactory(profile, CloudPrintProxyServiceFactoryForPolicyTest);

  TestingPrefService* prefs = profile->GetTestingPrefService();
  prefs->SetUserPref(prefs::kCloudPrintEmail,
                     Value::CreateStringValue(
                         MockServiceIPCServer::EnabledUserId()));

  CommandLine command_line(CommandLine::NO_PROGRAM);
  command_line.AppendSwitch(switches::kCheckCloudPrintConnectorPolicy);
  test_launcher_utils::PrepareBrowserCommandLineForTests(&command_line);

  WaitForConnect();
  MessageLoop::current()->PostDelayedTask(FROM_HERE,
                                          MessageLoop::QuitClosure(),
                                          TestTimeouts::action_timeout());

  bool run_loop = LaunchBrowser(command_line, profile);
  EXPECT_FALSE(run_loop);
  if (run_loop)
    MessageLoop::current()->Run();

  EXPECT_EQ(MockServiceIPCServer::EnabledUserId(),
            prefs->GetString(prefs::kCloudPrintEmail));

  ShutdownAndWaitForExitWithTimeout(handle);
  profile_manager.DeleteTestingProfile("StartBrowserWithoutPolicy");
}

TEST_F(CloudPrintProxyPolicyStartupTest, StartBrowserWithPolicy) {
  base::ProcessHandle handle =
      Launch("CloudPrintMockService_StartEnabledExpectDisabled");

  TestingBrowserProcess* browser_process =
      static_cast<TestingBrowserProcess*>(g_browser_process);
  TestingProfileManager profile_manager(browser_process);
  ASSERT_TRUE(profile_manager.SetUp());

  TestingProfile* profile =
      profile_manager.CreateTestingProfile("StartBrowserWithPolicy");
  CloudPrintProxyServiceFactory::GetInstance()->
      SetTestingFactory(profile, CloudPrintProxyServiceFactoryForPolicyTest);

  TestingPrefService* prefs = profile->GetTestingPrefService();
  prefs->SetUserPref(prefs::kCloudPrintEmail,
                     Value::CreateStringValue(
                         MockServiceIPCServer::EnabledUserId()));
  prefs->SetManagedPref(prefs::kCloudPrintProxyEnabled,
                        Value::CreateBooleanValue(false));

  CommandLine command_line(CommandLine::NO_PROGRAM);
  command_line.AppendSwitch(switches::kCheckCloudPrintConnectorPolicy);
  test_launcher_utils::PrepareBrowserCommandLineForTests(&command_line);

  WaitForConnect();
  MessageLoop::current()->PostDelayedTask(FROM_HERE,
                                          MessageLoop::QuitClosure(),
                                          TestTimeouts::action_timeout());

  bool run_loop = LaunchBrowser(command_line, profile);

  // No expectations on run_loop being true here; that would be a race
  // condition.
  if (run_loop)
    MessageLoop::current()->Run();

  EXPECT_EQ("", prefs->GetString(prefs::kCloudPrintEmail));

  ShutdownAndWaitForExitWithTimeout(handle);
  profile_manager.DeleteTestingProfile("StartBrowserWithPolicy");
}
