// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/debug/debugger.h"
#include "base/i18n/icu_util.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/process/launch.h"
#include "base/run_loop.h"
#include "base/sys_info.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/test/base/chrome_test_launcher.h"
#include "chrome/test/base/chrome_test_suite.h"
#include "chrome/test/base/mojo_test_connector.h"
#include "content/public/common/service_manager_connection.h"
#include "content/public/test/test_launcher.h"
#include "mash/package/mash_packaged_service.h"
#include "services/shell/public/cpp/connector.h"
#include "services/shell/public/cpp/service.h"
#include "services/shell/public/cpp/service_context.h"
#include "services/shell/public/cpp/service_runner.h"
#include "services/shell/runner/common/switches.h"
#include "services/shell/runner/host/child_process.h"
#include "services/shell/runner/host/child_process_base.h"
#include "services/shell/runner/init.h"

namespace {

void ConnectToDefaultApps(shell::Connector* connector) {
  connector->Connect("service:mash_session");
}

class MashTestSuite : public ChromeTestSuite {
 public:
  MashTestSuite(int argc, char** argv) : ChromeTestSuite(argc, argv) {}

  void SetMojoTestConnector(std::unique_ptr<MojoTestConnector> connector) {
    mojo_test_connector_ = std::move(connector);
  }
  MojoTestConnector* mojo_test_connector() {
    return mojo_test_connector_.get();
  }

 private:
  // ChromeTestSuite:
  void Shutdown() override {
    mojo_test_connector_.reset();
    ChromeTestSuite::Shutdown();
  }

  std::unique_ptr<MojoTestConnector> mojo_test_connector_;

  DISALLOW_COPY_AND_ASSIGN(MashTestSuite);
};

// Used to setup the command line for passing a mojo channel to tests.
class MashTestLauncherDelegate : public ChromeTestLauncherDelegate {
 public:
  MashTestLauncherDelegate() : ChromeTestLauncherDelegate(nullptr) {}
  ~MashTestLauncherDelegate() override {}

  MojoTestConnector* GetMojoTestConnectorForSingleProcess() {
    // This is only called for single process tests, in which case we need
    // the TestSuite to own the MojoTestConnector.
    DCHECK(base::CommandLine::ForCurrentProcess()->HasSwitch(
        content::kSingleProcessTestsFlag));
    DCHECK(test_suite_);
    test_suite_->SetMojoTestConnector(base::WrapUnique(new MojoTestConnector));
    return test_suite_->mojo_test_connector();
  }

 private:
  // ChromeTestLauncherDelegate:
  int RunTestSuite(int argc, char** argv) override {
    test_suite_.reset(new MashTestSuite(argc, argv));
    const int result = test_suite_->Run();
    test_suite_.reset();
    return result;
  }
  std::unique_ptr<content::TestState> PreRunTest(
      base::CommandLine* command_line,
      base::TestLauncher::LaunchOptions* test_launch_options) override {
    if (!mojo_test_connector_) {
      mojo_test_connector_ = base::MakeUnique<MojoTestConnector>();
      service_ = base::MakeUnique<mash::MashPackagedService>();
      service_->set_context(base::MakeUnique<shell::ServiceContext>(
          service_.get(), mojo_test_connector_->Init()));
    }
    std::unique_ptr<content::TestState> test_state =
        mojo_test_connector_->PrepareForTest(command_line, test_launch_options);
    // Start default apps after chrome, as they may try to connect to chrome on
    // startup. Attempt to connect once per test in case a previous test crashed
    // mash_session.
    ConnectToDefaultApps(service_->connector());
    return test_state;
  }
  void OnDoneRunningTests() override {
    // We have to shutdown this state here, while an AtExitManager is still
    // valid.
    service_.reset();
    mojo_test_connector_.reset();
  }

  std::unique_ptr<MashTestSuite> test_suite_;
  std::unique_ptr<MojoTestConnector> mojo_test_connector_;
  std::unique_ptr<mash::MashPackagedService> service_;

  DISALLOW_COPY_AND_ASSIGN(MashTestLauncherDelegate);
};

std::unique_ptr<content::ServiceManagerConnection>
    CreateServiceManagerConnection(MashTestLauncherDelegate* delegate) {
  std::unique_ptr<content::ServiceManagerConnection> connection(
      content::ServiceManagerConnection::Create(
          delegate->GetMojoTestConnectorForSingleProcess()->Init(),
          base::ThreadTaskRunnerHandle::Get()));
  connection->Start();
  ConnectToDefaultApps(connection->GetConnector());
  return connection;
}

void StartChildApp(shell::mojom::ServiceRequest service_request) {
  base::MessageLoop message_loop(base::MessageLoop::TYPE_DEFAULT);
  base::RunLoop run_loop;
  mash::MashPackagedService service;
  std::unique_ptr<shell::ServiceContext> context =
      base::MakeUnique<shell::ServiceContext>(&service,
                                              std::move(service_request));
  context->SetConnectionLostClosure(run_loop.QuitClosure());
  service.set_context(std::move(context));
  run_loop.Run();
}

}  // namespace

bool RunMashBrowserTests(int argc, char** argv, int* exit_code) {
  base::CommandLine::Init(argc, argv);
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  if (!command_line.HasSwitch("run-in-mash"))
    return false;

  if (command_line.HasSwitch(MojoTestConnector::kMashApp)) {
#if defined(OS_LINUX)
    base::AtExitManager exit_manager;
#endif
    base::i18n::InitializeICU();
    shell::ChildProcessMainWithCallback(base::Bind(&StartChildApp));
    *exit_code = 0;
    return true;
  }

  if (command_line.HasSwitch(switches::kChildProcess) &&
      !command_line.HasSwitch(MojoTestConnector::kTestSwitch)) {
    base::AtExitManager at_exit;
    shell::InitializeLogging();
    shell::WaitForDebuggerIfNecessary();
#if !defined(OFFICIAL_BUILD) && defined(OS_WIN)
    base::RouteStdioToConsole(false);
#endif
    *exit_code = shell::ChildProcessMain();
    return true;
  }

  int default_jobs = std::max(1, base::SysInfo::NumberOfProcessors() / 2);
  MashTestLauncherDelegate delegate;
  // --single_process and no primoridal pipe token indicate we were run directly
  // from the command line. In this case we have to start up
  // ServiceManagerConnection
  // as though we were embedded.
  content::ServiceManagerConnection::Factory shell_connection_factory;
  if (command_line.HasSwitch(content::kSingleProcessTestsFlag) &&
      !command_line.HasSwitch(switches::kPrimordialPipeToken)) {
    shell_connection_factory =
        base::Bind(&CreateServiceManagerConnection, &delegate);
    content::ServiceManagerConnection::SetFactoryForTest(
        &shell_connection_factory);
  }
  *exit_code = LaunchChromeTests(default_jobs, &delegate, argc, argv);
  return true;
}
