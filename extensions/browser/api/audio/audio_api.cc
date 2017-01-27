// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/audio/audio_api.h"

#include <utility>

#include "base/lazy_instance.h"
#include "base/values.h"
#include "extensions/browser/event_router.h"
#include "extensions/common/api/audio.h"

namespace extensions {

namespace audio = api::audio;

static base::LazyInstance<BrowserContextKeyedAPIFactory<AudioAPI> > g_factory =
    LAZY_INSTANCE_INITIALIZER;

// static
BrowserContextKeyedAPIFactory<AudioAPI>* AudioAPI::GetFactoryInstance() {
  return g_factory.Pointer();
}

AudioAPI::AudioAPI(content::BrowserContext* context)
    : browser_context_(context), service_(AudioService::CreateInstance()) {
  service_->AddObserver(this);
}

AudioAPI::~AudioAPI() {
  service_->RemoveObserver(this);
  delete service_;
  service_ = NULL;
}

AudioService* AudioAPI::GetService() const {
  return service_;
}

void AudioAPI::OnDeviceChanged() {
  if (EventRouter::Get(browser_context_)) {
    std::unique_ptr<Event> event(new Event(
        events::AUDIO_ON_DEVICE_CHANGED, audio::OnDeviceChanged::kEventName,
        std::unique_ptr<base::ListValue>(new base::ListValue())));
    EventRouter::Get(browser_context_)->BroadcastEvent(std::move(event));
  }
}

void AudioAPI::OnLevelChanged(const std::string& id, int level) {
  if (EventRouter::Get(browser_context_)) {
    std::unique_ptr<base::ListValue> args =
        audio::OnLevelChanged::Create(id, level);
    std::unique_ptr<Event> event(new Event(events::AUDIO_ON_LEVEL_CHANGED,
                                           audio::OnLevelChanged::kEventName,
                                           std::move(args)));
    EventRouter::Get(browser_context_)->BroadcastEvent(std::move(event));
  }
}

void AudioAPI::OnMuteChanged(bool is_input, bool is_muted) {
  EventRouter* event_router = EventRouter::Get(browser_context_);
  if (!event_router)
    return;

  // Dispatch onMuteChanged event.
  audio::MuteChangedEvent raw_event;
  raw_event.stream_type =
      is_input ? audio::STREAM_TYPE_INPUT : audio::STREAM_TYPE_OUTPUT;
  raw_event.is_muted = is_muted;
  std::unique_ptr<base::ListValue> event_args =
      audio::OnMuteChanged::Create(raw_event);
  std::unique_ptr<Event> event(new Event(events::AUDIO_ON_MUTE_CHANGED,
                                         audio::OnMuteChanged::kEventName,
                                         std::move(event_args)));
  event_router->BroadcastEvent(std::move(event));
}

void AudioAPI::OnDevicesChanged(const DeviceInfoList& devices) {
  if (EventRouter::Get(browser_context_)) {
    std::unique_ptr<base::ListValue> args =
        audio::OnDevicesChanged::Create(devices);
    std::unique_ptr<Event> event(new Event(events::AUDIO_ON_DEVICES_CHANGED,
                                           audio::OnDevicesChanged::kEventName,
                                           std::move(args)));
    EventRouter::Get(browser_context_)->BroadcastEvent(std::move(event));
  }
}

///////////////////////////////////////////////////////////////////////////////

ExtensionFunction::ResponseAction AudioGetInfoFunction::Run() {
  AudioService* service =
      AudioAPI::GetFactoryInstance()->Get(browser_context())->GetService();
  DCHECK(service);
  OutputInfo output_info;
  InputInfo input_info;
  if (!service->GetInfo(&output_info, &input_info)) {
    return RespondNow(
        Error("Error occurred when querying audio device information."));
  }

  return RespondNow(
      ArgumentList(audio::GetInfo::Results::Create(output_info, input_info)));
}

///////////////////////////////////////////////////////////////////////////////

ExtensionFunction::ResponseAction AudioSetActiveDevicesFunction::Run() {
  std::unique_ptr<audio::SetActiveDevices::Params> params(
      audio::SetActiveDevices::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  AudioService* service =
      AudioAPI::GetFactoryInstance()->Get(browser_context())->GetService();
  DCHECK(service);

  if (params->ids.as_device_id_lists) {
    if (!service->SetActiveDeviceLists(
            params->ids.as_device_id_lists->input,
            params->ids.as_device_id_lists->output)) {
      return RespondNow(Error("Failed to set active devices."));
    }
  } else if (params->ids.as_strings) {
    // TODO(tbarzic): This way of setting active devices is deprecated - have
    // this return error for apps that were not whitelisted for deprecated
    // version of audio API.
    service->SetActiveDevices(*params->ids.as_strings);
  }
  return RespondNow(NoArguments());
}

///////////////////////////////////////////////////////////////////////////////

ExtensionFunction::ResponseAction AudioSetPropertiesFunction::Run() {
  std::unique_ptr<audio::SetProperties::Params> params(
      audio::SetProperties::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  AudioService* service =
      AudioAPI::GetFactoryInstance()->Get(browser_context())->GetService();
  DCHECK(service);

  int volume_value = params->properties.volume.get() ?
      *params->properties.volume : -1;

  int gain_value = params->properties.gain.get() ?
      *params->properties.gain : -1;

  if (!service->SetDeviceSoundLevel(params->id, volume_value, gain_value))
    return RespondNow(Error("Could not set volume/gain properties"));

  if (params->properties.is_muted.get() &&
      !service->SetMuteForDevice(params->id, *params->properties.is_muted)) {
    return RespondNow(Error("Could not set mute property."));
  }

  return RespondNow(NoArguments());
}

///////////////////////////////////////////////////////////////////////////////

ExtensionFunction::ResponseAction AudioSetMuteFunction::Run() {
  std::unique_ptr<audio::SetMute::Params> params(
      audio::SetMute::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  AudioService* service =
      AudioAPI::GetFactoryInstance()->Get(browser_context())->GetService();
  DCHECK(service);

  if (!service->SetMute(params->stream_type == audio::STREAM_TYPE_INPUT,
                        params->is_muted)) {
    return RespondNow(Error("Could not set mute state."));
  }
  return RespondNow(NoArguments());
}

///////////////////////////////////////////////////////////////////////////////

ExtensionFunction::ResponseAction AudioGetMuteFunction::Run() {
  std::unique_ptr<audio::GetMute::Params> params(
      audio::GetMute::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  AudioService* service =
      AudioAPI::GetFactoryInstance()->Get(browser_context())->GetService();
  DCHECK(service);

  bool value = false;
  if (!service->GetMute(params->stream_type == audio::STREAM_TYPE_INPUT,
                        &value)) {
    return RespondNow(Error("Could not get mute state."));
  }
  return RespondNow(ArgumentList(audio::GetMute::Results::Create(value)));
}

}  // namespace extensions
