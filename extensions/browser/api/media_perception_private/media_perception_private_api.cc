// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/media_perception_private/media_perception_private_api.h"

namespace media_perception = extensions::api::media_perception_private;

namespace extensions {

using CallbackStatus = MediaPerceptionAPIManager::CallbackStatus;

namespace {

const char kErrorStringStatusDbusError[] = "Service is unreachable.";
const char kErrorStringStatusIdle[] = "Service is not running.";
const char kErrorStringStatusLaunching[] = "Service busy launching.";

std::string CallbackStatusToErrorMessage(const CallbackStatus& status) {
  switch (status) {
    case CallbackStatus::DBUS_ERROR:
      return kErrorStringStatusDbusError;
    case CallbackStatus::PROCESS_IDLE_ERROR:
      return kErrorStringStatusIdle;
    case CallbackStatus::PROCESS_LAUNCHING_ERROR:
      return kErrorStringStatusLaunching;
    case CallbackStatus::SUCCESS:
      return "CallbackStatus success.";
  }
  NOTREACHED() << "Reached CallbackStatus not in switch.";
  return "CallbackStatus string not found.";
}

}  // namespace

MediaPerceptionPrivateGetStateFunction ::
    MediaPerceptionPrivateGetStateFunction() {}

MediaPerceptionPrivateGetStateFunction ::
    ~MediaPerceptionPrivateGetStateFunction() {}

ExtensionFunction::ResponseAction
MediaPerceptionPrivateGetStateFunction::Run() {
  MediaPerceptionAPIManager* manager =
      MediaPerceptionAPIManager::Get(browser_context());
  manager->GetState(base::Bind(
      &MediaPerceptionPrivateGetStateFunction::GetStateCallback, this));
  return RespondLater();
}

void MediaPerceptionPrivateGetStateFunction::GetStateCallback(
    CallbackStatus status,
    media_perception::State state) {
  if (status != CallbackStatus::SUCCESS) {
    Respond(Error(CallbackStatusToErrorMessage(status)));
    return;
  }
  Respond(OneArgument(state.ToValue()));
}

MediaPerceptionPrivateSetStateFunction ::
    MediaPerceptionPrivateSetStateFunction() {}

MediaPerceptionPrivateSetStateFunction ::
    ~MediaPerceptionPrivateSetStateFunction() {}

ExtensionFunction::ResponseAction
MediaPerceptionPrivateSetStateFunction::Run() {
  std::unique_ptr<media_perception::SetState::Params> params =
      media_perception::SetState::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params.get());
  if (params->state.status != media_perception::STATUS_RUNNING &&
      params->state.status != media_perception::STATUS_SUSPENDED &&
      params->state.status != media_perception::STATUS_RESTARTING) {
    return RespondNow(
        Error("Status can only be set to RUNNING, SUSPENDED or RESTARTING."));
  }

  // Check that device context is only provided with SetState RUNNING.
  if (params->state.status != media_perception::STATUS_RUNNING &&
      params->state.device_context.get() != nullptr) {
    return RespondNow(
        Error("Only provide deviceContext with SetState RUNNING."));
  }
  MediaPerceptionAPIManager* manager =
      MediaPerceptionAPIManager::Get(browser_context());
  manager->SetState(
      params->state,
      base::Bind(&MediaPerceptionPrivateSetStateFunction::SetStateCallback,
                 this));
  return RespondLater();
}

void MediaPerceptionPrivateSetStateFunction::SetStateCallback(
    CallbackStatus status,
    media_perception::State state) {
  if (status != CallbackStatus::SUCCESS) {
    Respond(Error(CallbackStatusToErrorMessage(status)));
    return;
  }
  Respond(OneArgument(state.ToValue()));
}

MediaPerceptionPrivateGetDiagnosticsFunction ::
    MediaPerceptionPrivateGetDiagnosticsFunction() {}

MediaPerceptionPrivateGetDiagnosticsFunction ::
    ~MediaPerceptionPrivateGetDiagnosticsFunction() {}

ExtensionFunction::ResponseAction
MediaPerceptionPrivateGetDiagnosticsFunction::Run() {
  MediaPerceptionAPIManager* manager =
      MediaPerceptionAPIManager::Get(browser_context());
  manager->GetDiagnostics(base::Bind(
      &MediaPerceptionPrivateGetDiagnosticsFunction::GetDiagnosticsCallback,
      this));
  return RespondLater();
}

void MediaPerceptionPrivateGetDiagnosticsFunction::GetDiagnosticsCallback(
    CallbackStatus status,
    media_perception::Diagnostics diagnostics) {
  if (status != CallbackStatus::SUCCESS) {
    Respond(Error(CallbackStatusToErrorMessage(status)));
    return;
  }
  Respond(OneArgument(diagnostics.ToValue()));
}

}  // namespace extensions
