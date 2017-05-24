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
    callback.Run(CallbackStatus::PROCESS_LAUNCHING_ERROR,
                 media_perception::State());
    return;
  }

  // Calling getState with process not running returns State UNINITIALIZED.
  media_perception::State state_uninitialized;
  state_uninitialized.status = media_perception::STATUS_UNINITIALIZED;
  callback.Run(CallbackStatus::SUCCESS, std::move(state_uninitialized));
}

void MediaPerceptionAPIManager::SetState(const media_perception::State& state,
                                         const APIStateCallback& callback) {
  mri::State state_proto = StateIdlToProto(state);
  DCHECK(state_proto.status() == mri::State::RUNNING ||
         state_proto.status() == mri::State::SUSPENDED)
      << "Cannot set state to something other than RUNNING or SUSPENDED.";

  if (analytics_process_state_ == AnalyticsProcessState::RUNNING) {
    SetStateInternal(callback, state_proto);
    return;
  }

  if (analytics_process_state_ == AnalyticsProcessState::LAUNCHING) {
    callback.Run(CallbackStatus::PROCESS_LAUNCHING_ERROR,
                 media_perception::State());
    return;
  }

  // Analytics process is in state IDLE.
  if (state_proto.status() == mri::State::RUNNING) {
    analytics_process_state_ = AnalyticsProcessState::LAUNCHING;
    chromeos::UpstartClient* dbus_client =
        chromeos::DBusThreadManager::Get()->GetUpstartClient();
    dbus_client->StartMediaAnalytics(
        base::Bind(&MediaPerceptionAPIManager::UpstartCallback,
                   weak_ptr_factory_.GetWeakPtr(), callback, state_proto));
    return;
  }

  callback.Run(CallbackStatus::PROCESS_IDLE_ERROR, media_perception::State());
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

void MediaPerceptionAPIManager::UpstartCallback(
    const APIStateCallback& callback,
    const mri::State& state,
    bool succeeded) {
  if (!succeeded) {
    analytics_process_state_ = AnalyticsProcessState::IDLE;
    callback.Run(CallbackStatus::PROCESS_IDLE_ERROR, media_perception::State());
    return;
  }
  analytics_process_state_ = AnalyticsProcessState::RUNNING;
  SetStateInternal(callback, state);
}

void MediaPerceptionAPIManager::StateCallback(const APIStateCallback& callback,
                                              bool succeeded,
                                              const mri::State& state_proto) {
  media_perception::State state;
  if (!succeeded) {
    callback.Run(CallbackStatus::DBUS_ERROR, media_perception::State());
    return;
  }
  callback.Run(CallbackStatus::SUCCESS,
               media_perception::StateProtoToIdl(state_proto));
}

void MediaPerceptionAPIManager::GetDiagnosticsCallback(
    const APIGetDiagnosticsCallback& callback,
    bool succeeded,
    const mri::Diagnostics& diagnostics_proto) {
  if (!succeeded) {
    callback.Run(CallbackStatus::DBUS_ERROR, media_perception::Diagnostics());
    return;
  }
  callback.Run(CallbackStatus::SUCCESS,
               media_perception::DiagnosticsProtoToIdl(diagnostics_proto));
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
