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
#include "chrome/browser/ui/app_list/app_list_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "extensions/browser/event_router.h"

namespace extensions {

namespace hotword_private_constants {
const char kHotwordServiceUnavailable[] = "Hotword Service is unavailable.";
}  // hotword_private_constants

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
  pref_change_registrar_.Add(
      prefs::kHotwordAlwaysOnSearchEnabled,
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
  DCHECK(pref_name == std::string(prefs::kHotwordSearchEnabled) ||
         pref_name == std::string(prefs::kHotwordAlwaysOnSearchEnabled) ||
         pref_name == std::string(
             hotword_internal::kHotwordTrainingEnabled));
  SignalEvent(OnEnabledChanged::kEventName);
}

void HotwordPrivateEventService::OnHotwordSessionRequested() {
  SignalEvent(api::hotword_private::OnHotwordSessionRequested::kEventName);
}

void HotwordPrivateEventService::OnHotwordSessionStopped() {
  SignalEvent(api::hotword_private::OnHotwordSessionStopped::kEventName);
}

void HotwordPrivateEventService::OnFinalizeSpeakerModel() {
  SignalEvent(api::hotword_private::OnFinalizeSpeakerModel::kEventName);
}

void HotwordPrivateEventService::OnHotwordTriggered() {
  SignalEvent(api::hotword_private::OnHotwordTriggered::kEventName);
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
  scoped_ptr<api::hotword_private::SetAudioLoggingEnabled::Params> params(
      api::hotword_private::SetAudioLoggingEnabled::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  // TODO(kcarattini): Sync the chrome pref with the account-level
  // Audio History setting.
  PrefService* prefs = GetProfile()->GetPrefs();
  prefs->SetBoolean(prefs::kHotwordAudioLoggingEnabled, params->state);
  return true;
}

bool HotwordPrivateSetHotwordAlwaysOnSearchEnabledFunction::RunSync() {
  scoped_ptr<api::hotword_private::SetHotwordAlwaysOnSearchEnabled::Params>
      params(api::hotword_private::SetHotwordAlwaysOnSearchEnabled::Params::
      Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  PrefService* prefs = GetProfile()->GetPrefs();
  prefs->SetBoolean(prefs::kHotwordAlwaysOnSearchEnabled, params->state);
  return true;
}

bool HotwordPrivateGetStatusFunction::RunSync() {
  api::hotword_private::StatusDetails result;

  HotwordService* hotword_service =
      HotwordServiceFactory::GetForProfile(GetProfile());
  if (!hotword_service) {
    result.available = false;
  } else {
    result.available = hotword_service->IsServiceAvailable();
    result.audio_logging_enabled = hotword_service->IsOptedIntoAudioLogging();
    result.training_enabled = hotword_service->IsTraining();
  }

  PrefService* prefs = GetProfile()->GetPrefs();
  result.enabled_set = prefs->HasPrefPath(prefs::kHotwordSearchEnabled);
  result.enabled = prefs->GetBoolean(prefs::kHotwordSearchEnabled);
  result.always_on_enabled =
      prefs->GetBoolean(prefs::kHotwordAlwaysOnSearchEnabled);
  result.audio_logging_enabled = false;
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  result.experimental_hotword_enabled = command_line->HasSwitch(
      switches::kEnableExperimentalHotwording);

  SetResult(result.ToValue().release());
  return true;
}

bool HotwordPrivateSetHotwordSessionStateFunction::RunSync() {
  scoped_ptr<api::hotword_private::SetHotwordSessionState::Params> params(
      api::hotword_private::SetHotwordSessionState::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  HotwordService* hotword_service =
      HotwordServiceFactory::GetForProfile(GetProfile());
  if (hotword_service &&
      hotword_service->client() &&
      !hotword_service->IsTraining())
    hotword_service->client()->OnHotwordStateChanged(params->started);
  return true;
}

bool HotwordPrivateNotifyHotwordRecognitionFunction::RunSync() {
  HotwordService* hotword_service =
      HotwordServiceFactory::GetForProfile(GetProfile());
  if (hotword_service) {
    if (hotword_service->IsTraining()) {
      hotword_service->NotifyHotwordTriggered();
    } else if (hotword_service->client()) {
      hotword_service->client()->OnHotwordRecognized();
    } else if (HotwordService::IsExperimentalHotwordingEnabled() &&
               hotword_service->IsAlwaysOnEnabled()) {
      Browser* browser = GetCurrentBrowser();
      // If a Browser does not exist, fall back to the universally available,
      // but not recommended, way.
      AppListService* app_list_service = AppListService::Get(
          browser ? browser->host_desktop_type() : chrome::GetActiveDesktop());
      CHECK(app_list_service);
      app_list_service->ShowForVoiceSearch(GetProfile());
    }
  }
  return true;
}

bool HotwordPrivateGetLaunchStateFunction::RunSync() {
  HotwordService* hotword_service =
      HotwordServiceFactory::GetForProfile(GetProfile());
  if (!hotword_service) {
    error_ = hotword_private_constants::kHotwordServiceUnavailable;
    return false;
  }

  api::hotword_private::LaunchState result;
  result.launch_mode =
      hotword_service->GetHotwordAudioVerificationLaunchMode();
  SetResult(result.ToValue().release());
  return true;
}

bool HotwordPrivateStartTrainingFunction::RunSync() {
  HotwordService* hotword_service =
      HotwordServiceFactory::GetForProfile(GetProfile());
  if (!hotword_service) {
    error_ = hotword_private_constants::kHotwordServiceUnavailable;
    return false;
  }

  hotword_service->StartTraining();
  return true;
}

bool HotwordPrivateFinalizeSpeakerModelFunction::RunSync() {
  HotwordService* hotword_service =
      HotwordServiceFactory::GetForProfile(GetProfile());
  if (!hotword_service) {
    error_ = hotword_private_constants::kHotwordServiceUnavailable;
    return false;
  }

  hotword_service->FinalizeSpeakerModel();
  return true;
}

bool HotwordPrivateStopTrainingFunction::RunSync() {
  HotwordService* hotword_service =
      HotwordServiceFactory::GetForProfile(GetProfile());
  if (!hotword_service) {
    error_ = hotword_private_constants::kHotwordServiceUnavailable;
    return false;
  }

  hotword_service->StopTraining();
  return true;
}

}  // namespace extensions
