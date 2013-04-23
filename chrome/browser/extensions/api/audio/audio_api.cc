// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/audio/audio_api.h"

#include "base/lazy_instance.h"
#include "base/values.h"
#include "chrome/browser/extensions/event_names.h"
#include "chrome/browser/extensions/event_router.h"
#include "chrome/browser/extensions/extension_system.h"

namespace extensions {

static base::LazyInstance<ProfileKeyedAPIFactory<AudioAPI> >
g_factory = LAZY_INSTANCE_INITIALIZER;

// static
ProfileKeyedAPIFactory<AudioAPI>* AudioAPI::GetFactoryInstance() {
  return &g_factory.Get();
}

AudioAPI::AudioAPI(Profile* profile)
    : profile_(profile),
      service_(AudioService::CreateInstance()) {
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
  if (profile_ && ExtensionSystem::Get(profile_)->event_router()) {
    scoped_ptr<Event> event(new Event(event_names::kOnAudioDeviceChanged,
                                      scoped_ptr<ListValue>(new ListValue())));
    ExtensionSystem::Get(profile_)->event_router()->BroadcastEvent(
        event.Pass());
  }
}

bool AudioGetInfoFunction::RunImpl() {
  AudioService* service =
      AudioAPI::GetFactoryInstance()->GetForProfile(profile())->GetService();
  DCHECK(service);
  service->StartGetInfo(base::Bind(&AudioGetInfoFunction::OnGetInfoCompleted,
                                   this));
  return true;
}

void AudioGetInfoFunction::OnGetInfoCompleted(const OutputInfo& output_info,
                                              const InputInfo& input_info,
                                              bool success) {
  if (success)
    results_ = api::audio::GetInfo::Results::Create(output_info, input_info);
  else
    SetError("Error occured when querying audio device information.");
  SendResponse(success);
}

bool AudioSetActiveDevicesFunction::RunImpl() {
  // TODO: implement this.
  return false;
}

bool AudioSetPropertiesFunction::RunImpl() {
  // TODO: implement this.
  return false;
}

}  // namespace extensions
