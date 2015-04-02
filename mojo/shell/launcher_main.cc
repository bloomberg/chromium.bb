// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/strings/string_split.h"
#include "mojo/edk/embedder/embedder.h"
#include "mojo/edk/embedder/process_delegate.h"
#include "mojo/edk/embedder/simple_platform_support.h"
#include "mojo/shell/in_process_native_runner.h"
#include "mojo/shell/init.h"
#include "mojo/shell/native_application_support.h"
#include "url/gurl.h"

namespace mojo {
namespace shell {

const char kAppArgs[] = "app-args";
const char kAppPath[] = "app-path";
const char kAppURL[] = "app-url";
const char kShellPath[] = "shell-path";

class Launcher : public embedder::ProcessDelegate {
 public:
  explicit Launcher(const base::CommandLine& command_line)
      : app_path_(command_line.GetSwitchValuePath(kAppPath)),
        app_url_(command_line.GetSwitchValueASCII(kAppURL)),
        loop_(base::MessageLoop::TYPE_IO) {
    // TODO(vtl): I guess this should be SLAVE, not NONE?
    embedder::InitIPCSupport(embedder::ProcessType::NONE, loop_.task_runner(),
                             this, loop_.task_runner(),
                             embedder::ScopedPlatformHandle());

    base::SplitStringAlongWhitespace(command_line.GetSwitchValueASCII(kAppArgs),
                                     &app_args_);
  }

  ~Launcher() override {
    DCHECK(!application_request_.is_pending());

    embedder::ShutdownIPCSupportOnIOThread();
  }

  bool Connect() { return false; }

  bool Register() {
    return application_request_.is_pending();
  }

  void Run() {
    DCHECK(application_request_.is_pending());
    InProcessNativeRunner service_runner(nullptr);
    base::RunLoop run_loop;
    service_runner.Start(app_path_, NativeApplicationCleanup::DONT_DELETE,
                         application_request_.Pass(), run_loop.QuitClosure());
    run_loop.Run();
  }

 private:
  void OnRegistered(base::RunLoop* run_loop,
                    InterfaceRequest<Application> application_request) {
    application_request_ = application_request.Pass();
    run_loop->Quit();
  }

  // embedder::ProcessDelegate implementation:
  void OnShutdownComplete() override {
    NOTREACHED();  // Not called since we use ShutdownIPCSupportOnIOThread().
  }

  const base::FilePath app_path_;
  const GURL app_url_;
  std::vector<std::string> app_args_;
  base::MessageLoop loop_;
  InterfaceRequest<Application> application_request_;

  DISALLOW_COPY_AND_ASSIGN(Launcher);
};

}  // namespace shell
}  // namespace mojo

int main(int argc, char** argv) {
  base::AtExitManager at_exit;
  mojo::embedder::Init(
      make_scoped_ptr(new mojo::embedder::SimplePlatformSupport()));

  base::CommandLine::Init(argc, argv);
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  mojo::shell::InitializeLogging();

  mojo::shell::Launcher launcher(*command_line);
  if (!launcher.Connect()) {
    LOG(ERROR) << "Failed to connect on socket "
               << command_line->GetSwitchValueASCII(mojo::shell::kShellPath);
    return 1;
  }

  if (!launcher.Register()) {
    LOG(ERROR) << "Error registering "
               << command_line->GetSwitchValueASCII(mojo::shell::kAppURL);
    return 1;
  }

  launcher.Run();
  return 0;
}
