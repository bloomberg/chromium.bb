// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/hotword_private/hotword_private_api.h"

#include "base/lazy_instance.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/hotword_service.h"
#include "chrome/browser/search/hotword_service_factory.h"
#include "chrome/common/pref_names.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_system.h"

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
  SignalEvent();
}

void HotwordPrivateEventService::SignalEvent() {
  using OnEnabledChanged::kEventName;

  EventRouter* router = ExtensionSystem::Get(profile_)->event_router();
  if (!router || !router->HasEventListener(kEventName))
    return;
  scoped_ptr<base::ListValue> args(new base::ListValue());
  scoped_ptr<Event> event(new Event(kEventName, args.Pass()));
  router->BroadcastEvent(event.Pass());
}

bool HotwordPrivateSetEnabledFunction::RunImpl() {
  scoped_ptr<api::hotword_private::SetEnabled::Params> params(
      api::hotword_private::SetEnabled::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  PrefService* prefs = GetProfile()->GetPrefs();
  prefs->SetBoolean(prefs::kHotwordSearchEnabled, params->state);
  return true;
}

bool HotwordPrivateGetStatusFunction::RunImpl() {
  api::hotword_private::StatusDetails result;

  HotwordService* hotword_service =
      HotwordServiceFactory::GetForProfile(GetProfile());
  if (!hotword_service)
    result.available = false;
  else
    result.available = hotword_service->IsServiceAvailable();

  PrefService* prefs = GetProfile()->GetPrefs();
  result.enabled_set = prefs->HasPrefPath(prefs::kHotwordSearchEnabled);
  result.enabled =
      prefs->GetBoolean(prefs::kHotwordSearchEnabled);

  SetResult(result.ToValue().release());
  return true;
}

}  // namespace extensions
