// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/hotword_private/hotword_private_api.h"

#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/hotword_client.h"
#include "chrome/browser/search/hotword_service.h"
#include "chrome/browser/search/hotword_service_factory.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "extensions/browser/event_router.h"

namespace extensions {

namespace OnEnabledChanged =
    api::hotword_private::OnEnabledChanged;

static base::LazyInstance<
    BrowserContextKeyedAPIFactory<HotwordPrivateEventService> > g_factory =
    LAZY_INSTANCE_INITIALIZER;

HotwordPrivateEventService::HotwordPrivateEventService(
    content::BrowserContext* context)
    : profile_(Profile::FromBrowserContext(context)) {
  pref_change_registrar_.Init(profile_->GetPrefs());
  pref_change_registrar_.Add(
      prefs::kHotwordSearchEnabled,
      base::Bind(&HotwordPrivateEventService::OnEnabledChanged,
                 base::Unretained(this)));
}

HotwordPrivateEventService::~HotwordPrivateEventService() {
}

void HotwordPrivateEventService::Shutdown() {
}

// static
BrowserContextKeyedAPIFactory<HotwordPrivateEventService>*
HotwordPrivateEventService::GetFactoryInstance() {
  return g_factory.Pointer();
}

// static
const char* HotwordPrivateEventService::service_name() {
  return "HotwordPrivateEventService";
}

void HotwordPrivateEventService::OnEnabledChanged(
    const std::string& pref_name) {
  DCHECK_EQ(pref_name, std::string(prefs::kHotwordSearchEnabled));
  SignalEvent(OnEnabledChanged::kEventName);
}

void HotwordPrivateEventService::OnHotwordSessionRequested() {
  SignalEvent(api::hotword_private::OnHotwordSessionRequested::kEventName);
}

void HotwordPrivateEventService::OnHotwordSessionStopped() {
  SignalEvent(api::hotword_private::OnHotwordSessionStopped::kEventName);
}

void HotwordPrivateEventService::SignalEvent(const std::string& event_name) {
  EventRouter* router = EventRouter::Get(profile_);
  if (!router || !router->HasEventListener(event_name))
    return;
  scoped_ptr<base::ListValue> args(new base::ListValue());
  scoped_ptr<Event> event(new Event(event_name, args.Pass()));
  router->BroadcastEvent(event.Pass());
}

bool HotwordPrivateSetEnabledFunction::RunSync() {
  scoped_ptr<api::hotword_private::SetEnabled::Params> params(
      api::hotword_private::SetEnabled::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  PrefService* prefs = GetProfile()->GetPrefs();
  prefs->SetBoolean(prefs::kHotwordSearchEnabled, params->state);
  return true;
}

bool HotwordPrivateSetAudioLoggingEnabledFunction::RunSync() {
  scoped_ptr<api::hotword_private::SetEnabled::Params> params(
      api::hotword_private::SetEnabled::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  PrefService* prefs = GetProfile()->GetPrefs();
  prefs->SetBoolean(prefs::kHotwordAudioLoggingEnabled, params->state);
  return true;
}

bool HotwordPrivateGetStatusFunction::RunSync() {
  api::hotword_private::StatusDetails result;

  HotwordService* hotword_service =
      HotwordServiceFactory::GetForProfile(GetProfile());
  if (!hotword_service)
    result.available = false;
  else
    result.available = hotword_service->IsServiceAvailable();

  PrefService* prefs = GetProfile()->GetPrefs();
  result.enabled_set = prefs->HasPrefPath(prefs::kHotwordSearchEnabled);
  result.enabled = prefs->GetBoolean(prefs::kHotwordSearchEnabled);
  result.audio_logging_enabled = false;
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  result.experimental_hotword_enabled = command_line->HasSwitch(
      switches::kEnableExperimentalHotwording);
  if (hotword_service)
    result.audio_logging_enabled = hotword_service->IsOptedIntoAudioLogging();

  SetResult(result.ToValue().release());
  return true;
}

bool HotwordPrivateSetHotwordSessionStateFunction::RunSync() {
  scoped_ptr<api::hotword_private::SetHotwordSessionState::Params> params(
      api::hotword_private::SetHotwordSessionState::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  HotwordService* hotword_service =
      HotwordServiceFactory::GetForProfile(GetProfile());
  if (hotword_service && hotword_service->client())
    hotword_service->client()->OnHotwordStateChanged(params->started);
  return true;
}

bool HotwordPrivateNotifyHotwordRecognitionFunction::RunSync() {
  HotwordService* hotword_service =
      HotwordServiceFactory::GetForProfile(GetProfile());
  if (hotword_service && hotword_service->client())
    hotword_service->client()->OnHotwordRecognized();
  return true;
}

}  // namespace extensions
