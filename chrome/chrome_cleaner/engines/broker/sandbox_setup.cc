// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/engines/broker/sandbox_setup.h"

#include <windows.h>

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/chrome_cleaner/constants/chrome_cleaner_switches.h"
#include "chrome/chrome_cleaner/engines/common/engine_result_codes.h"
#include "chrome/chrome_cleaner/settings/settings.h"
#include "mojo/public/cpp/system/message_pipe.h"

namespace chrome_cleaner {

EngineSandboxSetupHooks::EngineSandboxSetupHooks(
    scoped_refptr<EngineClient> engine_client)
    : engine_client_(engine_client) {}

EngineSandboxSetupHooks::~EngineSandboxSetupHooks() = default;

ResultCode EngineSandboxSetupHooks::UpdateSandboxPolicy(
    sandbox::TargetPolicy* policy,
    base::CommandLine* command_line) {
  // Create a Mojo message pipe to talk to the sandbox target process.
  mojo::ScopedMessagePipeHandle mojo_pipe =
      SetupSandboxMessagePipe(policy, command_line);

  engine_client_->PostBindEngineCommandsPtr(std::move(mojo_pipe));

  // Propagate engine selection switches to the sandbox target.
  command_line->AppendSwitchNative(
      kEngineSwitch, base::NumberToString16(Settings::GetInstance()->engine()));

  return RESULT_CODE_SUCCESS;
}

ResultCode EngineSandboxSetupHooks::TargetResumed() {
  DCHECK(engine_client_);
  uint32_t engine_result = engine_client_->Initialize();
  if (engine_result != EngineResultCode::kSuccess) {
    LOG(DFATAL) << "Engine initialize failed with 0x" << std::hex
                << engine_result << std::dec;
    return RESULT_CODE_ENGINE_INITIALIZATION_FAILED;
  }
  return RESULT_CODE_SUCCESS;
}

std::pair<ResultCode, scoped_refptr<EngineClient>> SpawnEngineSandbox(
    Engine::Name engine_name,
    RegistryLogger* const registry_logger,
    scoped_refptr<MojoTaskRunner> mojo_task_runner,
    const SandboxConnectionErrorCallback& connection_error_callback,
    std::unique_ptr<InterfaceMetadataObserver> interface_metadata_observer) {
  scoped_refptr<EngineClient> engine_client = EngineClient::CreateEngineClient(
      engine_name,
      base::BindRepeating(&RegistryLogger::WriteExperimentalEngineResultCode,
                          base::Unretained(registry_logger)),
      connection_error_callback, mojo_task_runner,
      std::move(interface_metadata_observer));

  EngineSandboxSetupHooks mojo_setup_hooks(engine_client.get());
  ResultCode result_code =
      SpawnSandbox(&mojo_setup_hooks, SandboxType::kEngine);
  return std::make_pair(result_code, engine_client);
}

}  // namespace chrome_cleaner
