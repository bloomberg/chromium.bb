// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/app_shim/app_shim_host_manager_mac.h"

#include <unistd.h>

#include "apps/app_shim/app_shim_messages.h"
#include "apps/app_shim/test/app_shim_host_manager_test_api_mac.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/mac/app_mode_common.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/test_utils.h"
#include "ipc/ipc_channel_proxy.h"
#include "ipc/ipc_listener.h"
#include "ipc/ipc_message.h"

namespace {

const char kTestAppMode[] = "test_app";

// A test version of the AppShimController IPC client in chrome_main_app_mode.
class TestShimClient : public IPC::Listener {
 public:
  TestShimClient(const base::FilePath& socket_path);
  virtual ~TestShimClient();

  template <class T>
  void Send(const T& message) {
    channel_->Send(message);
  }

 private:
  // IPC::Listener overrides:
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual void OnChannelError() OVERRIDE;

  base::Thread io_thread_;
  scoped_ptr<IPC::ChannelProxy> channel_;

  DISALLOW_COPY_AND_ASSIGN(TestShimClient);
};

TestShimClient::TestShimClient(const base::FilePath& socket_path)
    : io_thread_("TestShimClientIO") {
  base::Thread::Options io_thread_options;
  io_thread_options.message_loop_type = base::MessageLoop::TYPE_IO;
  io_thread_.StartWithOptions(io_thread_options);

  IPC::ChannelHandle handle(socket_path.value());
  channel_.reset(new IPC::ChannelProxy(handle, IPC::Channel::MODE_NAMED_CLIENT,
      this, io_thread_.message_loop_proxy().get()));
}

TestShimClient::~TestShimClient() {}

bool TestShimClient::OnMessageReceived(const IPC::Message& message) {
  return true;
}

void TestShimClient::OnChannelError() {
  // Client should not get any channel errors for the current set of tests.
  PLOG(FATAL) << "ChannelError";
}

// Browser Test for AppShimHostManager to test IPC interactions across the
// UNIX domain socket.
class AppShimHostManagerBrowserTest : public InProcessBrowserTest,
                                      public apps::AppShimHandler {
 public:
  AppShimHostManagerBrowserTest();
  virtual ~AppShimHostManagerBrowserTest();

 protected:
  // Wait for OnShimLaunch, then send a quit, and wait for the response. Used to
  // test launch behavior.
  void RunAndExitGracefully();

  // InProcessBrowserTest overrides:
  virtual void SetUpOnMainThread() OVERRIDE;
  virtual void TearDownOnMainThread() OVERRIDE;
  virtual bool SetUpUserDataDirectory() OVERRIDE;

  // AppShimHandler overrides:
  virtual void OnShimLaunch(apps::AppShimHandler::Host* host,
                            apps::AppShimLaunchType launch_type,
                            const std::vector<base::FilePath>& files) OVERRIDE;
  virtual void OnShimClose(apps::AppShimHandler::Host* host) OVERRIDE {}
  virtual void OnShimFocus(apps::AppShimHandler::Host* host,
                           apps::AppShimFocusType focus_type,
                           const std::vector<base::FilePath>& files) OVERRIDE {}
  virtual void OnShimSetHidden(apps::AppShimHandler::Host* host,
                               bool hidden) OVERRIDE {}
  virtual void OnShimQuit(apps::AppShimHandler::Host* host) OVERRIDE;

  scoped_ptr<TestShimClient> test_client_;
  base::FilePath short_socket_path_;
  std::vector<base::FilePath> last_launch_files_;
  apps::AppShimLaunchType last_launch_type_;

private:
  scoped_refptr<content::MessageLoopRunner> runner_;
  base::ScopedTempDir short_temp_dir_;

  int launch_count_;
  int quit_count_;

  DISALLOW_COPY_AND_ASSIGN(AppShimHostManagerBrowserTest);
};

AppShimHostManagerBrowserTest::AppShimHostManagerBrowserTest()
    : last_launch_type_(apps::APP_SHIM_LAUNCH_NUM_TYPES),
      launch_count_(0),
      quit_count_(0) {
}

AppShimHostManagerBrowserTest::~AppShimHostManagerBrowserTest() {
}

void AppShimHostManagerBrowserTest::RunAndExitGracefully() {
  runner_ = new content::MessageLoopRunner();
  EXPECT_EQ(0, launch_count_);
  runner_->Run();  // Will stop in OnShimLaunch().
  EXPECT_EQ(1, launch_count_);

  runner_ = new content::MessageLoopRunner();
  test_client_->Send(new AppShimHostMsg_QuitApp);
  EXPECT_EQ(0, quit_count_);
  runner_->Run();  // Will stop in OnShimQuit().
  EXPECT_EQ(1, quit_count_);

  test_client_.reset();
}

void AppShimHostManagerBrowserTest::SetUpOnMainThread() {
  // Can't do this in the constructor, it needs a BrowserProcess.
  apps::AppShimHandler::RegisterHandler(kTestAppMode, this);
}

void AppShimHostManagerBrowserTest::TearDownOnMainThread() {
  apps::AppShimHandler::RemoveHandler(kTestAppMode);
}

bool AppShimHostManagerBrowserTest::SetUpUserDataDirectory() {
  // Create a symlink at /tmp/scoped_dir_XXXXXX/udd that points to the real user
  // data dir, and use this as the domain socket path. This is required because
  // there is a path length limit for named sockets that is exceeded in
  // multi-process test spawning.
  base::FilePath real_user_data_dir;
  EXPECT_TRUE(PathService::Get(chrome::DIR_USER_DATA, &real_user_data_dir));
  EXPECT_TRUE(
      short_temp_dir_.CreateUniqueTempDirUnderPath(base::FilePath("/tmp")));
  base::FilePath shortened_user_data_dir = short_temp_dir_.path().Append("udd");
  EXPECT_EQ(0, ::symlink(real_user_data_dir.AsUTF8Unsafe().c_str(),
                         shortened_user_data_dir.AsUTF8Unsafe().c_str()));

  test::AppShimHostManagerTestApi::OverrideUserDataDir(shortened_user_data_dir);
  short_socket_path_ =
      shortened_user_data_dir.Append(app_mode::kAppShimSocketName);

  return InProcessBrowserTest::SetUpUserDataDirectory();
}

void AppShimHostManagerBrowserTest::OnShimLaunch(
    apps::AppShimHandler::Host* host,
    apps::AppShimLaunchType launch_type,
    const std::vector<base::FilePath>& files) {
  host->OnAppLaunchComplete(apps::APP_SHIM_LAUNCH_SUCCESS);
  ++launch_count_;
  last_launch_type_ = launch_type;
  last_launch_files_ = files;
  runner_->Quit();
}

void AppShimHostManagerBrowserTest::OnShimQuit(
    apps::AppShimHandler::Host* host) {
  ++quit_count_;
  runner_->Quit();
}

// Test regular launch, which would ask Chrome to launch the app.
IN_PROC_BROWSER_TEST_F(AppShimHostManagerBrowserTest, LaunchNormal) {
  test_client_.reset(new TestShimClient(short_socket_path_));
  test_client_->Send(new AppShimHostMsg_LaunchApp(
      browser()->profile()->GetPath(),
      kTestAppMode,
      apps::APP_SHIM_LAUNCH_NORMAL,
      std::vector<base::FilePath>()));

  RunAndExitGracefully();
  EXPECT_EQ(apps::APP_SHIM_LAUNCH_NORMAL, last_launch_type_);
  EXPECT_TRUE(last_launch_files_.empty());
}

// Test register-only launch, used when Chrome has already launched the app.
IN_PROC_BROWSER_TEST_F(AppShimHostManagerBrowserTest, LaunchRegisterOnly) {
  test_client_.reset(new TestShimClient(short_socket_path_));
  test_client_->Send(new AppShimHostMsg_LaunchApp(
      browser()->profile()->GetPath(),
      kTestAppMode,
      apps::APP_SHIM_LAUNCH_REGISTER_ONLY,
      std::vector<base::FilePath>()));

  RunAndExitGracefully();
  EXPECT_EQ(apps::APP_SHIM_LAUNCH_REGISTER_ONLY, last_launch_type_);
  EXPECT_TRUE(last_launch_files_.empty());
}

// Ensure the domain socket can be created in a fresh user data dir.
IN_PROC_BROWSER_TEST_F(AppShimHostManagerBrowserTest,
                       PRE_ReCreate) {
  test::AppShimHostManagerTestApi test_api(
      g_browser_process->platform_part()->app_shim_host_manager());
  EXPECT_TRUE(test_api.factory());
}

// Ensure the domain socket can be re-created after a prior browser process has
// quit.
IN_PROC_BROWSER_TEST_F(AppShimHostManagerBrowserTest,
                       ReCreate) {
  test::AppShimHostManagerTestApi test_api(
      g_browser_process->platform_part()->app_shim_host_manager());
  EXPECT_TRUE(test_api.factory());
}

// Test for AppShimHostManager that fails to create the socket.
class AppShimHostManagerBrowserTestFailsCreate :
    public AppShimHostManagerBrowserTest {
 public:
  AppShimHostManagerBrowserTestFailsCreate() {}

 private:
  virtual bool SetUpUserDataDirectory() OVERRIDE;

  base::ScopedTempDir barrier_dir_;

  DISALLOW_COPY_AND_ASSIGN(AppShimHostManagerBrowserTestFailsCreate);
};

bool AppShimHostManagerBrowserTestFailsCreate::SetUpUserDataDirectory() {
  base::FilePath user_data_dir;
  // Start in the "real" user data dir for this test. This is a meta-test for
  // the symlinking steps used in the superclass. That is, by putting the
  // clobber in the actual user data dir, the test will fail if the symlink
  // does not actually point to the user data dir, since it won't be clobbered.
  EXPECT_TRUE(PathService::Get(chrome::DIR_USER_DATA, &user_data_dir));
  base::FilePath socket_path =
      user_data_dir.Append(app_mode::kAppShimSocketName);
  // Create a "barrier" to forming the UNIX domain socket. This is just a
  // pre-existing directory which can not be unlink()ed, in order to place a
  // named socked there instead.
  EXPECT_TRUE(barrier_dir_.Set(socket_path));
  return AppShimHostManagerBrowserTest::SetUpUserDataDirectory();
}

// Test error handling. This is essentially testing for lifetime correctness
// during startup for unexpected failures.
IN_PROC_BROWSER_TEST_F(AppShimHostManagerBrowserTestFailsCreate,
                       SocketFailure) {
  test::AppShimHostManagerTestApi test_api(
      g_browser_process->platform_part()->app_shim_host_manager());
  EXPECT_FALSE(test_api.factory());
}

}  // namespace
