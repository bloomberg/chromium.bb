// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_MOJO_TEST_CONNECTOR_H_
#define CHROME_TEST_BASE_MOJO_TEST_CONNECTOR_H_

#include <memory>

#include "base/macros.h"
#include "base/process/process_handle.h"
#include "base/test/launcher/test_launcher.h"
#include "services/service_manager/background/background_service_manager.h"
#include "services/service_manager/public/interfaces/service.mojom.h"

namespace base {
class CommandLine;
class Thread;
class Value;
}

namespace mojo {
namespace edk {
class ScopedIPCSupport;
}
}

namespace content {
class TestState;
}

// MojoTestConnector is responsible for providing the necessary wiring for
// test processes to get a mojo channel passed to them.  To use this class
// call PrepareForTest() prior to launching each test. It is expected
// PrepareForTest() is called from content::TestLauncherDelegate::PreRunTest().
class MojoTestConnector {
 public:
  // Switch added to command line of each test.
  static const char kTestSwitch[];

  // Command line switch added to all apps that are expected to be provided by
  // browser_tests.
  static const char kMashApp[];

  // Enumeration of the possible chrome-ash configurations supported by this
  // test.
  enum class Config {
    // Aura is backed by mus, but chrome and ash are still in the same process.
    MUS,

    // Aura is backed by mus and chrome and ash are in separate processes. In
    // this mode chrome code can only use ash code in ash/public/cpp.
    MASH,
  };

  MojoTestConnector(std::unique_ptr<base::Value> catalog_contents,
                    Config config);
  ~MojoTestConnector();

  service_manager::BackgroundServiceManager* background_service_manager() {
    return background_service_manager_.get();
  }

  // Initializes the Mojo environment, and IPC thread.
  void Init();

  // Initializes the background thread the ServiceManager runs on.
  service_manager::mojom::ServiceRequest InitBackgroundServiceManager();

  std::unique_ptr<content::TestState> PrepareForTest(
      base::CommandLine* command_line,
      base::TestLauncher::LaunchOptions* test_launch_options,
      base::OnceClosure on_process_launched);

  void StartService(const std::string& service_name);

 private:
  class ServiceProcessLauncherDelegateImpl;

  const Config config_;

  std::unique_ptr<ServiceProcessLauncherDelegateImpl>
      service_process_launcher_delegate_;
  std::unique_ptr<service_manager::BackgroundServiceManager>
      background_service_manager_;

  std::unique_ptr<base::Thread> ipc_thread_;
  std::unique_ptr<mojo::edk::ScopedIPCSupport> ipc_support_;

  std::unique_ptr<base::Value> catalog_contents_;

  DISALLOW_COPY_AND_ASSIGN(MojoTestConnector);
};

#endif  // CHROME_TEST_BASE_MOJO_TEST_CONNECTOR_H_
