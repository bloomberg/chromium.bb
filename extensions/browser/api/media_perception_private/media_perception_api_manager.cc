// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/media_perception_private/media_perception_api_manager.h"

#include "base/lazy_instance.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/media_analytics_client.h"
#include "chromeos/dbus/upstart_client.h"
#include "extensions/browser/api/media_perception_private/conversion_utils.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_function.h"

namespace media_perception = extensions::api::media_perception_private;

namespace extensions {

namespace {

media_perception::State GetStateForServiceError(
    const media_perception::ServiceError service_error) {
  media_perception::State state;
  state.status = media_perception::STATUS_SERVICE_ERROR;
  state.service_error = service_error;
  return state;
}

media_perception::Diagnostics GetDiagnosticsForServiceError(
    const media_perception::ServiceError& service_error) {
  media_perception::Diagnostics diagnostics;
  diagnostics.service_error = service_error;
  return diagnostics;
}

}  // namespace

// static
MediaPerceptionAPIManager* MediaPerceptionAPIManager::Get(
    content::BrowserContext* context) {
  return GetFactoryInstance()->Get(context);
}

static base::LazyInstance<
    BrowserContextKeyedAPIFactory<MediaPerceptionAPIManager>>::Leaky g_factory =
    LAZY_INSTANCE_INITIALIZER;

// static
BrowserContextKeyedAPIFactory<MediaPerceptionAPIManager>*
MediaPerceptionAPIManager::GetFactoryInstance() {
  return g_factory.Pointer();
}

MediaPerceptionAPIManager::MediaPerceptionAPIManager(
    content::BrowserContext* context)
    : browser_context_(context),
      analytics_process_state_(AnalyticsProcessState::IDLE),
      weak_ptr_factory_(this) {
  chromeos::MediaAnalyticsClient* dbus_client =
      chromeos::DBusThreadManager::Get()->GetMediaAnalyticsClient();
  dbus_client->SetMediaPerceptionSignalHandler(
      base::Bind(&MediaPerceptionAPIManager::MediaPerceptionSignalHandler,
                 weak_ptr_factory_.GetWeakPtr()));
}

MediaPerceptionAPIManager::~MediaPerceptionAPIManager() {
  chromeos::MediaAnalyticsClient* dbus_client =
      chromeos::DBusThreadManager::Get()->GetMediaAnalyticsClient();
  dbus_client->ClearMediaPerceptionSignalHandler();
  // Stop the separate media analytics process.
  chromeos::UpstartClient* upstart_client =
      chromeos::DBusThreadManager::Get()->GetUpstartClient();
  upstart_client->StopMediaAnalytics();
}

void MediaPerceptionAPIManager::GetState(const APIStateCallback& callback) {
  if (analytics_process_state_ == AnalyticsProcessState::RUNNING) {
    chromeos::MediaAnalyticsClient* dbus_client =
        chromeos::DBusThreadManager::Get()->GetMediaAnalyticsClient();
    dbus_client->GetState(base::Bind(&MediaPerceptionAPIManager::StateCallback,
                                     weak_ptr_factory_.GetWeakPtr(), callback));
    return;
  }

  if (analytics_process_state_ == AnalyticsProcessState::LAUNCHING) {
    callback.Run(GetStateForServiceError(
        media_perception::SERVICE_ERROR_SERVICE_BUSY_LAUNCHING));
    return;
  }

  // Calling getState with process not running returns State UNINITIALIZED.
  media_perception::State state_uninitialized;
  state_uninitialized.status = media_perception::STATUS_UNINITIALIZED;
  callback.Run(std::move(state_uninitialized));
}

void MediaPerceptionAPIManager::SetState(const media_perception::State& state,
                                         const APIStateCallback& callback) {
  mri::State state_proto = StateIdlToProto(state);
  DCHECK(state_proto.status() == mri::State::RUNNING ||
         state_proto.status() == mri::State::SUSPENDED ||
         state_proto.status() == mri::State::RESTARTING)
      << "Cannot set state to something other than RUNNING, SUSPENDED "
         "or RESTARTING.";

  if (analytics_process_state_ == AnalyticsProcessState::LAUNCHING) {
    callback.Run(GetStateForServiceError(
        media_perception::SERVICE_ERROR_SERVICE_BUSY_LAUNCHING));
    return;
  }

  // If the media analytics process is running or not and restart is requested,
  // then send restart upstart command.
  if (state_proto.status() == mri::State::RESTARTING) {
    analytics_process_state_ = AnalyticsProcessState::LAUNCHING;
    chromeos::UpstartClient* dbus_client =
        chromeos::DBusThreadManager::Get()->GetUpstartClient();
    dbus_client->RestartMediaAnalytics(
        base::Bind(&MediaPerceptionAPIManager::UpstartRestartCallback,
                   weak_ptr_factory_.GetWeakPtr(), callback));
    return;
  }

  if (analytics_process_state_ == AnalyticsProcessState::RUNNING) {
    SetStateInternal(callback, state_proto);
    return;
  }

  // Analytics process is in state IDLE.
  if (state_proto.status() == mri::State::RUNNING) {
    analytics_process_state_ = AnalyticsProcessState::LAUNCHING;
    chromeos::UpstartClient* dbus_client =
        chromeos::DBusThreadManager::Get()->GetUpstartClient();
    dbus_client->StartMediaAnalytics(
        base::Bind(&MediaPerceptionAPIManager::UpstartStartCallback,
                   weak_ptr_factory_.GetWeakPtr(), callback, state_proto));
    return;
  }

  callback.Run(GetStateForServiceError(
      media_perception::SERVICE_ERROR_SERVICE_NOT_RUNNING));
}

void MediaPerceptionAPIManager::SetStateInternal(
    const APIStateCallback& callback,
    const mri::State& state) {
  chromeos::MediaAnalyticsClient* dbus_client =
      chromeos::DBusThreadManager::Get()->GetMediaAnalyticsClient();
  dbus_client->SetState(state,
                        base::Bind(&MediaPerceptionAPIManager::StateCallback,
                                   weak_ptr_factory_.GetWeakPtr(), callback));
}

void MediaPerceptionAPIManager::GetDiagnostics(
    const APIGetDiagnosticsCallback& callback) {
  chromeos::MediaAnalyticsClient* dbus_client =
      chromeos::DBusThreadManager::Get()->GetMediaAnalyticsClient();
  dbus_client->GetDiagnostics(
      base::Bind(&MediaPerceptionAPIManager::GetDiagnosticsCallback,
                 weak_ptr_factory_.GetWeakPtr(), callback));
}

void MediaPerceptionAPIManager::UpstartStartCallback(
    const APIStateCallback& callback,
    const mri::State& state,
    bool succeeded) {
  if (!succeeded) {
    analytics_process_state_ = AnalyticsProcessState::IDLE;
    callback.Run(GetStateForServiceError(
        media_perception::SERVICE_ERROR_SERVICE_NOT_RUNNING));
    return;
  }
  analytics_process_state_ = AnalyticsProcessState::RUNNING;
  SetStateInternal(callback, state);
}

void MediaPerceptionAPIManager::UpstartRestartCallback(
    const APIStateCallback& callback,
    bool succeeded) {
  if (!succeeded) {
    analytics_process_state_ = AnalyticsProcessState::IDLE;
    callback.Run(GetStateForServiceError(
        media_perception::SERVICE_ERROR_SERVICE_NOT_RUNNING));
    return;
  }
  analytics_process_state_ = AnalyticsProcessState::RUNNING;
  GetState(callback);
}

void MediaPerceptionAPIManager::StateCallback(const APIStateCallback& callback,
                                              bool succeeded,
                                              const mri::State& state_proto) {
  media_perception::State state;
  if (!succeeded) {
    callback.Run(GetStateForServiceError(
        media_perception::SERVICE_ERROR_SERVICE_UNREACHABLE));
    return;
  }
  callback.Run(media_perception::StateProtoToIdl(state_proto));
}

void MediaPerceptionAPIManager::GetDiagnosticsCallback(
    const APIGetDiagnosticsCallback& callback,
    bool succeeded,
    const mri::Diagnostics& diagnostics_proto) {
  if (!succeeded) {
    callback.Run(GetDiagnosticsForServiceError(
        media_perception::SERVICE_ERROR_SERVICE_UNREACHABLE));
    return;
  }
  callback.Run(media_perception::DiagnosticsProtoToIdl(diagnostics_proto));
}

void MediaPerceptionAPIManager::MediaPerceptionSignalHandler(
    const mri::MediaPerception& media_perception_proto) {
  EventRouter* router = EventRouter::Get(browser_context_);
  DCHECK(router) << "EventRouter is null.";

  media_perception::MediaPerception media_perception =
      media_perception::MediaPerceptionProtoToIdl(media_perception_proto);
  std::unique_ptr<Event> event(
      new Event(events::MEDIA_PERCEPTION_PRIVATE_ON_MEDIA_PERCEPTION,
                media_perception::OnMediaPerception::kEventName,
                media_perception::OnMediaPerception::Create(media_perception)));
  router->BroadcastEvent(std::move(event));
}

}  // namespace extensions
