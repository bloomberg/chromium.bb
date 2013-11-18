// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/message_loop/message_loop.h"
#include "gin/initialize.h"
#include "gin/modules/module_runner_delegate.h"
#include "gin/test/file_runner.h"
#include "gin/try_catch.h"

namespace gin {
namespace {

std::string Load(const base::FilePath& path) {
  std::string source;
  if (!ReadFileToString(path, &source))
    LOG(FATAL) << "Unable to read " << path.LossyDisplayName();
  return source;
}

void Run(base::WeakPtr<Runner> runner, const std::string& source) {
  if (!runner)
    return;
  Runner::Scope scope(runner.get());
  runner->Run(source);
}

base::FilePath GetModuleBase() {
  base::FilePath module_base;
  CHECK(file_util::GetCurrentDirectory(&module_base));
  return module_base;
}

class ShellRunnerDelegate : public ModuleRunnerDelegate {
 public:
  ShellRunnerDelegate() : ModuleRunnerDelegate(GetModuleBase()) {}

  virtual void UnhandledException(Runner* runner,
                                  TryCatch& try_catch) OVERRIDE {
    ModuleRunnerDelegate::UnhandledException(runner, try_catch);
    LOG(ERROR) << try_catch.GetPrettyMessage();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ShellRunnerDelegate);
};

}  // namespace
}  // namespace gin

int main(int argc, char** argv) {
  base::AtExitManager at_exit;
  CommandLine::Init(argc, argv);
  gin::Initialize();

  base::MessageLoop message_loop;

  gin::ShellRunnerDelegate delegate;
  gin::Runner runner(&delegate, v8::Isolate::GetCurrent());

  CommandLine::StringVector args = CommandLine::ForCurrentProcess()->GetArgs();
  for (CommandLine::StringVector::const_iterator it = args.begin();
       it != args.end(); ++it) {
    base::MessageLoop::current()->PostTask(FROM_HERE, base::Bind(
        gin::Run, runner.GetWeakPtr(), gin::Load(base::FilePath(*it))));
  }

  message_loop.RunUntilIdle();
  return 0;
}
