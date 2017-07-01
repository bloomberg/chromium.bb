// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/debug/debugger.h"
#include "base/debug/stack_trace.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/i18n/icu_util.h"
#include "base/json/json_reader.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/run_loop.h"
#include "base/sys_info.h"
#include "base/task_scheduler/task_scheduler.h"
#include "base/test/launcher/test_launcher.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "chrome/app/mash/embedded_services.h"
#include "chrome/test/base/chrome_test_launcher.h"
#include "chrome/test/base/chrome_test_suite.h"
#include "chrome/test/base/mojo_test_connector.h"
#include "content/public/app/content_main.h"
#include "content/public/common/service_manager_connection.h"
#include "content/public/test/test_launcher.h"
#include "mash/session/public/interfaces/constants.mojom.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/service_context.h"
#include "services/service_manager/public/cpp/service_runner.h"
#include "services/service_manager/public/cpp/standalone_service/standalone_service.h"
#include "services/service_manager/runner/common/switches.h"
#include "services/service_manager/runner/init.h"
#include "services/ui/common/switches.h"
#include "ui/aura/env.h"

namespace {

const base::FilePath::CharType kMashCatalogFilename[] =
    FILE_PATH_LITERAL("mash_browser_tests_catalog.json");
const base::FilePath::CharType kMusCatalogFilename[] =
    FILE_PATH_LITERAL("mus_browser_tests_catalog.json");

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
  MashTestLauncherDelegate() : ChromeTestLauncherDelegate(nullptr) {
    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    config_ = command_line->HasSwitch("run-in-mash")
                  ? MojoTestConnector::Config::MASH
                  : MojoTestConnector::Config::MUS;
  }
  ~MashTestLauncherDelegate() override {}

  MojoTestConnector* GetMojoTestConnectorForSingleProcess() {
    // This is only called for single process tests, in which case we need
    // the TestSuite to own the MojoTestConnector.
    DCHECK(base::CommandLine::ForCurrentProcess()->HasSwitch(
        content::kSingleProcessTestsFlag));
    DCHECK(test_suite_);
    test_suite_->SetMojoTestConnector(
        base::MakeUnique<MojoTestConnector>(ReadCatalogManifest(), config_));
    return test_suite_->mojo_test_connector();
  }

 private:
  // ChromeTestLauncherDelegate:
  int RunTestSuite(int argc, char** argv) override {
    test_suite_.reset(new MashTestSuite(argc, argv));
    content::GetContentMainParams()->env_mode = aura::Env::Mode::MUS;
    const int result = test_suite_->Run();
    test_suite_.reset();
    return result;
  }

  std::unique_ptr<content::TestState> PreRunTest(
      base::CommandLine* command_line,
      base::TestLauncher::LaunchOptions* test_launch_options) override {
    if (!mojo_test_connector_) {
      mojo_test_connector_ =
          base::MakeUnique<MojoTestConnector>(ReadCatalogManifest(), config_);
      mojo_test_connector_->Init();
    }
    return mojo_test_connector_->PrepareForTest(command_line,
                                                test_launch_options);
  }

  void OnDoneRunningTests() override {
    // We have to shutdown this state here, while an AtExitManager is still
    // valid.
    mojo_test_connector_.reset();
  }

  std::unique_ptr<base::Value> ReadCatalogManifest() {
    std::string catalog_contents;
    base::FilePath exe_path;
    base::PathService::Get(base::DIR_EXE, &exe_path);
    base::FilePath catalog_path = exe_path.Append(
        config_ == MojoTestConnector::Config::MASH ? kMashCatalogFilename
                                                   : kMusCatalogFilename);
    bool result = base::ReadFileToString(catalog_path, &catalog_contents);
    DCHECK(result);
    std::unique_ptr<base::Value> manifest_value =
        base::JSONReader::Read(catalog_contents);
    DCHECK(manifest_value);
    return manifest_value;
  }

  MojoTestConnector::Config config_;

  std::unique_ptr<MashTestSuite> test_suite_;
  std::unique_ptr<MojoTestConnector> mojo_test_connector_;

  DISALLOW_COPY_AND_ASSIGN(MashTestLauncherDelegate);
};

std::unique_ptr<content::ServiceManagerConnection>
    CreateServiceManagerConnection(MashTestLauncherDelegate* delegate) {
  delegate->GetMojoTestConnectorForSingleProcess()->Init();
  std::unique_ptr<content::ServiceManagerConnection> connection(
      content::ServiceManagerConnection::Create(
          delegate->GetMojoTestConnectorForSingleProcess()
              ->InitBackgroundServiceManager(),
          base::ThreadTaskRunnerHandle::Get()));
  connection->Start();
  connection->GetConnector()->StartService(mash::session::mojom::kServiceName);
  return connection;
}

void StartEmbeddedService(service_manager::mojom::ServiceRequest request) {
  // The UI service requires this to be TYPE_UI, so we just always use TYPE_UI
  // for now.
  base::MessageLoop message_loop(base::MessageLoop::TYPE_UI);
  base::RunLoop run_loop;
  service_manager::ServiceContext context(
      CreateEmbeddedMashService(
          base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
              service_manager::switches::kServiceName)),
      std::move(request));
  context.SetQuitClosure(run_loop.QuitClosure());
  run_loop.Run();
}

}  // namespace

bool RunMashBrowserTests(int argc, char** argv, int* exit_code) {
  base::CommandLine::Init(argc, argv);
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch("run-in-mash") &&
      !command_line->HasSwitch("run-in-mus")) {
    return false;
  }

  if (command_line->HasSwitch(MojoTestConnector::kMashApp)) {
#if defined(OS_LINUX)
    base::AtExitManager exit_manager;
#endif
    base::i18n::InitializeICU();

#if !defined(OFFICIAL_BUILD)
    base::debug::EnableInProcessStackDumping();
#endif

    base::TaskScheduler::CreateAndStartWithDefaultParams("StandaloneService");

    command_line->AppendSwitch(ui::switches::kUseTestConfig);
    service_manager::RunStandaloneService(base::Bind(&StartEmbeddedService));
    *exit_code = 0;

    base::TaskScheduler::GetInstance()->Shutdown();

    return true;
  }

  size_t parallel_jobs = base::NumParallelJobs();
  if (parallel_jobs > 1U) {
    parallel_jobs /= 2U;
  }
  MashTestLauncherDelegate delegate;
  // --single_process and no service pipe token indicate we were run directly
  // from the command line. In this case we have to start up
  // ServiceManagerConnection as though we were embedded.
  content::ServiceManagerConnection::Factory service_manager_connection_factory;
  if (command_line->HasSwitch(content::kSingleProcessTestsFlag) &&
      !command_line->HasSwitch(service_manager::switches::kServicePipeToken)) {
    service_manager_connection_factory =
        base::Bind(&CreateServiceManagerConnection, &delegate);
    content::ServiceManagerConnection::SetFactoryForTest(
        &service_manager_connection_factory);
  }
  *exit_code = LaunchChromeTests(parallel_jobs, &delegate, argc, argv);
  return true;
}
