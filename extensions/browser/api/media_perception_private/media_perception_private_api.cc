// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/media_perception_private/media_perception_private_api.h"
#include "base/bind.h"

namespace extensions {

MediaPerceptionPrivateGetStateFunction ::
    MediaPerceptionPrivateGetStateFunction() {}

MediaPerceptionPrivateGetStateFunction ::
    ~MediaPerceptionPrivateGetStateFunction() {}

ExtensionFunction::ResponseAction
MediaPerceptionPrivateGetStateFunction::Run() {
  MediaPerceptionAPIManager* manager =
      MediaPerceptionAPIManager::Get(browser_context());
  manager->GetState(base::BindOnce(
      &MediaPerceptionPrivateGetStateFunction::GetStateCallback, this));
  return RespondLater();
}

void MediaPerceptionPrivateGetStateFunction::GetStateCallback(
    extensions::api::media_perception_private::State state) {
  Respond(OneArgument(state.ToValue()));
}

MediaPerceptionPrivateSetStateFunction ::
    MediaPerceptionPrivateSetStateFunction() {}

MediaPerceptionPrivateSetStateFunction ::
    ~MediaPerceptionPrivateSetStateFunction() {}

ExtensionFunction::ResponseAction
MediaPerceptionPrivateSetStateFunction::Run() {
  std::unique_ptr<extensions::api::media_perception_private::SetState::Params>
      params =
          extensions::api::media_perception_private::SetState::Params::Create(
              *args_);
  EXTENSION_FUNCTION_VALIDATE(params.get());
  if (params->state.status !=
          extensions::api::media_perception_private::STATUS_RUNNING &&
      params->state.status !=
          extensions::api::media_perception_private::STATUS_SUSPENDED &&
      params->state.status !=
          extensions::api::media_perception_private::STATUS_RESTARTING &&
      params->state.status !=
          extensions::api::media_perception_private::STATUS_STOPPED) {
    return RespondNow(
        Error("Status can only be set to RUNNING, SUSPENDED, RESTARTING, or "
              "STOPPED."));
  }

  // Check that device context is only provided with SetState RUNNING.
  if (params->state.status !=
          extensions::api::media_perception_private::STATUS_RUNNING &&
      params->state.device_context.get() != nullptr) {
    return RespondNow(
        Error("Only provide deviceContext with SetState RUNNING."));
  }

  // Check that video stream parameters are only provided with SetState RUNNING.
  if (params->state.status !=
          extensions::api::media_perception_private::STATUS_RUNNING &&
      params->state.video_stream_param.get() != nullptr) {
    return RespondNow(
        Error("SetState: status must be RUNNING to set videoStreamParam."));
  }

  // Check that configuration is only provided with SetState RUNNING.
  if (params->state.configuration &&
      params->state.status !=
          extensions::api::media_perception_private::STATUS_RUNNING) {
    return RespondNow(Error("Status must be RUNNING to set configuration."));
  }

  // Check that whiteboard configuration is only provided with SetState RUNNING.
  if (params->state.whiteboard &&
      params->state.status !=
          extensions::api::media_perception_private::STATUS_RUNNING) {
    return RespondNow(Error(
        "Status must be RUNNING to set whiteboard configuration."));
  }

  MediaPerceptionAPIManager* manager =
      MediaPerceptionAPIManager::Get(browser_context());
  manager->SetState(
      params->state,
      base::BindOnce(&MediaPerceptionPrivateSetStateFunction::SetStateCallback,
                     this));
  return RespondLater();
}

void MediaPerceptionPrivateSetStateFunction::SetStateCallback(
    extensions::api::media_perception_private::State state) {
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
    extensions::api::media_perception_private::Diagnostics diagnostics) {
  Respond(OneArgument(diagnostics.ToValue()));
}

MediaPerceptionPrivateSetAnalyticsComponentFunction::
    MediaPerceptionPrivateSetAnalyticsComponentFunction() {}

MediaPerceptionPrivateSetAnalyticsComponentFunction::
    ~MediaPerceptionPrivateSetAnalyticsComponentFunction() {}

ExtensionFunction::ResponseAction
MediaPerceptionPrivateSetAnalyticsComponentFunction::Run() {
  std::unique_ptr<
      extensions::api::media_perception_private::SetAnalyticsComponent::Params>
      params = extensions::api::media_perception_private::
          SetAnalyticsComponent::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params.get());

  MediaPerceptionAPIManager* manager =
      MediaPerceptionAPIManager::Get(browser_context());
  manager->SetAnalyticsComponent(
      params->component,
      base::BindOnce(&MediaPerceptionPrivateSetAnalyticsComponentFunction::
                         OnAnalyticsComponentSet,
                     this));
  return RespondLater();
}

void MediaPerceptionPrivateSetAnalyticsComponentFunction::
    OnAnalyticsComponentSet(
        extensions::api::media_perception_private::ComponentState
            component_state) {
  Respond(OneArgument(component_state.ToValue()));
}

MediaPerceptionPrivateSetComponentProcessStateFunction::
    MediaPerceptionPrivateSetComponentProcessStateFunction() = default;

MediaPerceptionPrivateSetComponentProcessStateFunction::
    ~MediaPerceptionPrivateSetComponentProcessStateFunction() = default;

ExtensionFunction::ResponseAction
MediaPerceptionPrivateSetComponentProcessStateFunction::Run() {
  std::unique_ptr<extensions::api::media_perception_private::
                      SetComponentProcessState::Params>
      params = extensions::api::media_perception_private::
          SetComponentProcessState::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params.get());
  if (params->process_state.status !=
          extensions::api::media_perception_private::PROCESS_STATUS_STARTED &&
      params->process_state.status !=
          extensions::api::media_perception_private::PROCESS_STATUS_STOPPED) {
    return RespondNow(
        Error("Cannot set process_state to something other than STARTED or "
              "STOPPED."));
  }

  MediaPerceptionAPIManager* manager =
      MediaPerceptionAPIManager::Get(browser_context());
  manager->SetComponentProcessState(
      params->process_state,
      base::BindOnce(&MediaPerceptionPrivateSetComponentProcessStateFunction::
                         OnComponentProcessStateSet,
                     this));
  return did_respond() ? AlreadyResponded() : RespondLater();
}

void MediaPerceptionPrivateSetComponentProcessStateFunction::
    OnComponentProcessStateSet(
        extensions::api::media_perception_private::ProcessState process_state) {
  Respond(OneArgument(process_state.ToValue()));
}

}  // namespace extensions
