// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Create a service process that uses a Mock to respond to the browser in order
// to test launching the browser using the cloud print policy check command
// line switch.

#include <stdint.h>

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/message_loop/message_loop.h"
#include "base/process/kill.h"
#include "base/process/process.h"
#include "base/rand_util.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/waitable_event.h"
#include "base/task_scheduler/post_task.h"
#include "base/test/multiprocess_test.h"
#include "base/test/test_timeouts.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/default_tick_clock.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/service_process/service_process_control.h"
#include "chrome/browser/ui/startup/startup_browser_creator.h"
#include "chrome/common/chrome_content_client.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/service_messages.h"
#include "chrome/common/service_process_util.h"
#include "chrome/service/service_ipc_server.h"
#include "chrome/service/service_process.h"
#include "chrome/test/base/chrome_unit_test_suite.h"
#include "chrome/test/base/test_launcher_utils.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_io_thread_state.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/browser/notification_service.h"
#include "content/public/common/content_paths.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_utils.h"
#include "ipc/ipc_channel_mojo.h"
#include "mojo/edk/embedder/embedder.h"
#include "mojo/edk/embedder/named_platform_handle.h"
#include "mojo/edk/embedder/named_platform_handle_utils.h"
#include "mojo/edk/embedder/peer_connection.h"
#include "mojo/edk/embedder/scoped_ipc_support.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/multiprocess_func_list.h"

#if defined(OS_MACOSX)
#include "chrome/common/mac/mock_launchd.h"
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
using content::BrowserThread;

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

class TestStartupClientChannelListener : public IPC::Listener {
 public:
  bool OnMessageReceived(const IPC::Message& message) override { return false; }
};

void ConnectAsync(mojo::ScopedMessagePipeHandle handle,
                  mojo::edk::NamedPlatformHandle os_pipe,
                  mojo::edk::PeerConnection* peer_connection) {
  mojo::edk::ScopedPlatformHandle os_pipe_handle =
      mojo::edk::CreateClientHandle(os_pipe);
  if (!os_pipe_handle.is_valid())
    return;

  mojo::FuseMessagePipes(
      peer_connection->Connect(mojo::edk::ConnectionParams(
          mojo::edk::TransportProtocol::kLegacy, std::move(os_pipe_handle))),
      std::move(handle));
}

const char kProcessChannelID[] = "process-channel-id";

}  // namespace

class TestServiceProcess : public ServiceProcess {
 public:
  TestServiceProcess() { }
  ~TestServiceProcess() override {}

  bool Initialize(base::MessageLoopForUI* message_loop,
                  ServiceProcessState* state);
};

bool TestServiceProcess::Initialize(base::MessageLoopForUI* message_loop,
                                    ServiceProcessState* state) {
  main_message_loop_ = message_loop;

  service_process_state_.reset(state);

  base::Thread::Options options(base::MessageLoop::TYPE_IO, 0);
  io_thread_.reset(new base::Thread("TestServiceProcess_IO"));
  return io_thread_->StartWithOptions(options);
}

// This mocks the service side IPC message handler, allowing us to have a
// minimal service process.
class MockServiceIPCServer : public ServiceIPCServer {
 public:
  static std::string EnabledUserId();

  MockServiceIPCServer(
      ServiceIPCServer::Client* client,
      const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner,
      base::WaitableEvent* shutdown_event)
      : ServiceIPCServer(client, io_task_runner, shutdown_event),
        enabled_(true) {}

  MOCK_METHOD1(OnMessageReceived, bool(const IPC::Message& message));
  MOCK_METHOD1(OnChannelConnected, void(int32_t peer_pid));
  MOCK_METHOD0(OnChannelError, void());

  void SetServiceEnabledExpectations();
  void SetWillBeDisabledExpectations();

  void CallServiceOnChannelConnected(int32_t peer_pid) {
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
              OnMessageReceived(Property(
                  &IPC::Message::type,
                  static_cast<int32_t>(ServiceMsg_GetCloudPrintProxyInfo::ID))))
      .Times(AnyNumber())
      .WillRepeatedly(
          WithoutArgs(Invoke(this, &MockServiceIPCServer::SendInfo)));

  EXPECT_CALL(*this, OnMessageReceived(Property(
                         &IPC::Message::type,
                         static_cast<int32_t>(ServiceMsg_Shutdown::ID))))
      .Times(1)
      .WillOnce(DoAll(
          Assign(&g_good_shutdown, true),
          WithoutArgs(Invoke(g_service_process, &ServiceProcess::Shutdown)),
          Return(true)));
}

void MockServiceIPCServer::SetWillBeDisabledExpectations() {
  SetServiceEnabledExpectations();

  EXPECT_CALL(*this,
              OnMessageReceived(Property(
                  &IPC::Message::type,
                  static_cast<int32_t>(ServiceMsg_DisableCloudPrintProxy::ID))))
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
  base::PlatformThread::SetName("Main Thread");
  base::MessageLoopForUI main_message_loop;
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  content::RegisterPathProvider();

  base::FilePath user_data_dir =
      command_line->GetSwitchValuePath(switches::kUserDataDir);
  CHECK(!user_data_dir.empty());
  CHECK(test_launcher_utils::OverrideUserDataDir(user_data_dir));

#if defined(OS_MACOSX)
  if (!command_line->HasSwitch(kTestExecutablePath))
    return kMissingSwitch;
  base::FilePath executable_path =
      command_line->GetSwitchValuePath(kTestExecutablePath);
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

  // Needed for IPC.
  mojo::edk::Init();
  mojo::edk::ScopedIPCSupport ipc_support(
      service_process.io_task_runner(),
      mojo::edk::ScopedIPCSupport::ShutdownPolicy::FAST);

  MockServiceIPCServer server(&service_process,
                              service_process.io_task_runner(),
                              service_process.GetShutdownEventForTesting());

  // Here is where the expectations/mock responses need to be set up.
  set_expectations.Run(&server);

  EXPECT_TRUE(server.Init());
  EXPECT_TRUE(state->SignalReady(service_process.io_task_runner().get(),
                                 base::Bind(&ShutdownTask)));
#if defined(OS_MACOSX)
  mock_launchd.SignalReady();
#endif

  // Connect up the parent/child IPC channel to signal that the test can
  // continue.
  TestStartupClientChannelListener listener;
  EXPECT_TRUE(base::CommandLine::ForCurrentProcess()->HasSwitch(
      kProcessChannelID));
  std::string startup_channel_name =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          kProcessChannelID);
  mojo::edk::PeerConnection peer_connection;
  std::unique_ptr<IPC::ChannelProxy> startup_channel =
      IPC::ChannelProxy::Create(
          peer_connection
              .Connect(mojo::edk::ConnectionParams(
                  mojo::edk::TransportProtocol::kLegacy,
                  mojo::edk::CreateClientHandle(
                      mojo::edk::NamedPlatformHandle(startup_channel_name))))
              .release(),
          IPC::Channel::MODE_CLIENT, &listener,
          service_process.io_task_runner());

  base::RunLoop().Run();
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
                                         public IPC::Listener {
 public:
  CloudPrintProxyPolicyStartupTest();
  ~CloudPrintProxyPolicyStartupTest() override;

  void SetUp() override;
  void TearDown() override;

  scoped_refptr<base::SingleThreadTaskRunner> IOTaskRunner() {
    return BrowserThread::GetTaskRunnerForThread(BrowserThread::IO);
  }
  base::Process Launch(const std::string& name);
  void WaitForConnect(mojo::edk::PeerConnection* peer_connection);
  bool Send(IPC::Message* message);
  void ShutdownAndWaitForExitWithTimeout(base::Process process);

  // IPC::Listener implementation
  bool OnMessageReceived(const IPC::Message& message) override { return false; }
  void OnChannelConnected(int32_t peer_pid) override;

  // MultiProcessTest implementation.
  base::CommandLine MakeCmdLine(const std::string& procname) override;

  bool LaunchBrowser(const base::CommandLine& command_line, Profile* profile) {
    StartupBrowserCreator browser_creator;
    return browser_creator.ProcessCmdLineImpl(
        command_line, base::FilePath(), false, profile,
        StartupBrowserCreator::Profiles());
  }

 protected:
  content::TestBrowserThreadBundle thread_bundle_;
  base::ScopedTempDir temp_user_data_dir_;

  mojo::edk::NamedPlatformHandle startup_channel_handle_;
  mojo::edk::PeerConnection peer_connection_;
  std::unique_ptr<IPC::ChannelProxy> startup_channel_;
  std::unique_ptr<ChromeContentClient> content_client_;
  std::unique_ptr<ChromeContentBrowserClient> browser_content_client_;

#if defined(OS_MACOSX)
  base::ScopedTempDir temp_dir_;
  base::FilePath executable_path_, bundle_path_;
  std::unique_ptr<MockLaunchd> mock_launchd_;
  std::unique_ptr<Launchd::ScopedInstance> scoped_launchd_instance_;
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
      content::RunMessageLoop();
    }

    void Notify() {
      seen_ = true;
      if (running_)
        base::MessageLoopForUI::current()->QuitWhenIdle();
    }

   private:
    bool seen_;
    bool running_;
  };

  WindowedChannelConnectionObserver observer_;
};

CloudPrintProxyPolicyStartupTest::CloudPrintProxyPolicyStartupTest()
    : thread_bundle_(content::TestBrowserThreadBundle::REAL_IO_THREAD) {
  // Although is really a unit test which runs in the browser_tests binary, it
  // doesn't get the unit setup which normally happens in the unit test binary.
  ChromeUnitTestSuite::InitializeProviders();
  ChromeUnitTestSuite::InitializeResourceBundle();
}

CloudPrintProxyPolicyStartupTest::~CloudPrintProxyPolicyStartupTest() {
}

void CloudPrintProxyPolicyStartupTest::SetUp() {
  content_client_.reset(new ChromeContentClient);
  content::SetContentClient(content_client_.get());
  browser_content_client_.reset(new ChromeContentBrowserClient());
  content::SetBrowserClientForTesting(browser_content_client_.get());

  TestingBrowserProcess::CreateInstance();
  // Ensure test does not use the standard profile directory. This is copied
  // from InProcessBrowserTest::SetUp(). These tests require a more complex
  // process startup so they are unable to just inherit from
  // InProcessBrowserTest.
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  base::FilePath user_data_dir =
      command_line->GetSwitchValuePath(switches::kUserDataDir);
  if (user_data_dir.empty()) {
    ASSERT_TRUE(temp_user_data_dir_.CreateUniqueTempDir() &&
                temp_user_data_dir_.IsValid())
        << "Could not create temporary user data directory \""
        << temp_user_data_dir_.GetPath().value() << "\".";

    user_data_dir = temp_user_data_dir_.GetPath();
    command_line->AppendSwitchPath(switches::kUserDataDir, user_data_dir);
  }
  ASSERT_TRUE(test_launcher_utils::OverrideUserDataDir(user_data_dir));

#if defined(OS_MACOSX)
  EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());
  EXPECT_TRUE(MockLaunchd::MakeABundle(temp_dir_.GetPath(),
                                       "CloudPrintProxyTest", &bundle_path_,
                                       &executable_path_));
  mock_launchd_.reset(new MockLaunchd(executable_path_,
                                      base::MessageLoopForUI::current(),
                                      true, false));
  scoped_launchd_instance_.reset(
      new Launchd::ScopedInstance(mock_launchd_.get()));
#endif
}

void CloudPrintProxyPolicyStartupTest::TearDown() {
  browser_content_client_.reset();
  content_client_.reset();
  content::SetContentClient(NULL);

  TestingBrowserProcess::DeleteInstance();
}

base::Process CloudPrintProxyPolicyStartupTest::Launch(
    const std::string& name) {
  EXPECT_FALSE(CheckServiceProcessReady());

  startup_channel_handle_ = mojo::edk::NamedPlatformHandle(
      base::StringPrintf("%d.%p.%d", base::GetCurrentProcId(), this,
                         base::RandInt(0, std::numeric_limits<int>::max())));
  startup_channel_ = IPC::ChannelProxy::Create(
      peer_connection_
          .Connect(mojo::edk::ConnectionParams(
              mojo::edk::TransportProtocol::kLegacy,
              mojo::edk::CreateServerHandle(startup_channel_handle_)))
          .release(),
      IPC::Channel::MODE_SERVER, this, IOTaskRunner());

  base::SpawnChildResult spawn_result = SpawnChild(name);
  EXPECT_TRUE(spawn_result.process.IsValid());
  return std::move(spawn_result.process);
}

void CloudPrintProxyPolicyStartupTest::WaitForConnect(
    mojo::edk::PeerConnection* peer_connection) {
  observer_.Wait();
  EXPECT_TRUE(CheckServiceProcessReady());
  EXPECT_TRUE(base::ThreadTaskRunnerHandle::Get().get());

  mojo::MessagePipe pipe;
  base::PostTaskWithTraits(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BACKGROUND},
      base::BindOnce(&ConnectAsync, base::Passed(&pipe.handle1),
                     GetServiceProcessChannel(), peer_connection));
  ServiceProcessControl::GetInstance()->SetChannel(
      IPC::ChannelProxy::Create(IPC::ChannelMojo::CreateClientFactory(
                                    std::move(pipe.handle0), IOTaskRunner()),
                                this, IOTaskRunner()));
}

bool CloudPrintProxyPolicyStartupTest::Send(IPC::Message* message) {
  return ServiceProcessControl::GetInstance()->Send(message);
}

void CloudPrintProxyPolicyStartupTest::ShutdownAndWaitForExitWithTimeout(
    base::Process process) {
  ASSERT_TRUE(Send(new ServiceMsg_Shutdown()));

  int exit_code = -100;
  bool exited = process.WaitForExitWithTimeout(TestTimeouts::action_timeout(),
                                               &exit_code);
  EXPECT_TRUE(exited);
  EXPECT_EQ(exit_code, 0);
}

void CloudPrintProxyPolicyStartupTest::OnChannelConnected(int32_t peer_pid) {
  observer_.Notify();
}

base::CommandLine CloudPrintProxyPolicyStartupTest::MakeCmdLine(
    const std::string& procname) {
  base::CommandLine cl = MultiProcessTest::MakeCmdLine(procname);
  cl.AppendSwitchNative(kProcessChannelID, startup_channel_handle_.name);
#if defined(OS_MACOSX)
  cl.AppendSwitchASCII(kTestExecutablePath, executable_path_.value());
#endif
  return cl;
}

TEST_F(CloudPrintProxyPolicyStartupTest, StartAndShutdown) {
  mojo::edk::Init();
  mojo::edk::ScopedIPCSupport ipc_support(
      BrowserThread::GetTaskRunnerForThread(BrowserThread::IO),
      mojo::edk::ScopedIPCSupport::ShutdownPolicy::FAST);

  TestingBrowserProcess* browser_process =
      TestingBrowserProcess::GetGlobal();
  TestingProfileManager profile_manager(browser_process);
  ASSERT_TRUE(profile_manager.SetUp());

  // Must be created after the TestingProfileManager since that creates the
  // LocalState for the BrowserProcess.  Must be created before profiles are
  // constructed.
  chrome::TestingIOThreadState testing_io_thread_state;

  base::Process process =
      Launch("CloudPrintMockService_StartEnabledWaitForQuit");
  mojo::edk::PeerConnection peer_connection;
  WaitForConnect(&peer_connection);
  ShutdownAndWaitForExitWithTimeout(std::move(process));
  ServiceProcessControl::GetInstance()->Disconnect();
  content::RunAllPendingInMessageLoop();
}
