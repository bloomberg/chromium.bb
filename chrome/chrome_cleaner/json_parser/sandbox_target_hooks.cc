// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/json_parser/sandbox_target_hooks.h"

#include <utility>

#include "chrome/chrome_cleaner/constants/chrome_cleaner_switches.h"
#include "chrome/chrome_cleaner/os/early_exit.h"

namespace chrome_cleaner {

JsonParserSandboxTargetHooks::JsonParserSandboxTargetHooks(
    MojoTaskRunner* mojo_task_runner)
    : mojo_task_runner_(mojo_task_runner) {}

JsonParserSandboxTargetHooks::~JsonParserSandboxTargetHooks() {
  // Ensure Mojo objects are deleted on the IPC thread.
  mojo_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(
                     [](std::unique_ptr<JsonParserImpl> json_parser_impl) {
                       json_parser_impl.reset();
                     },
                     base::Passed(&json_parser_impl_)));
}

ResultCode JsonParserSandboxTargetHooks::TargetDroppedPrivileges(
    const base::CommandLine& command_line) {
  mojom::JsonParserRequest request(ExtractSandboxMessagePipe(command_line));

  // This loop will run forever. Once the communication channel with the broker
  // process is broken, mojo error handler will abort this process.
  base::RunLoop run_loop;
  mojo_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&JsonParserSandboxTargetHooks::CreateJsonParserImpl,
                     base::Unretained(this), base::Passed(&request)));
  run_loop.Run();

  return RESULT_CODE_SUCCESS;
}

void JsonParserSandboxTargetHooks::CreateJsonParserImpl(
    mojom::JsonParserRequest request) {
  json_parser_impl_ = std::make_unique<JsonParserImpl>(
      std::move(request), base::BindOnce(&EarlyExit, 1));
}

ResultCode RunJsonParserSandbox(const base::CommandLine& command_line,
                                sandbox::TargetServices* target_services) {
  scoped_refptr<MojoTaskRunner> mojo_task_runner = MojoTaskRunner::Create();
  JsonParserSandboxTargetHooks target_hooks(mojo_task_runner.get());

  return RunSandboxTarget(command_line, target_services, &target_hooks);
}

}  // namespace chrome_cleaner
