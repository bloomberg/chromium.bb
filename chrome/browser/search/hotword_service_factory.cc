// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/hotword_service_factory.h"

#include "base/command_line.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/hotword_service.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserContext;
using content::BrowserThread;

// static
HotwordService* HotwordServiceFactory::GetForProfile(BrowserContext* context) {
  return static_cast<HotwordService*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
HotwordServiceFactory* HotwordServiceFactory::GetInstance() {
  return base::Singleton<HotwordServiceFactory>::get();
}

// static
bool HotwordServiceFactory::IsServiceAvailable(BrowserContext* context) {
  HotwordService* hotword_service = GetForProfile(context);
  return hotword_service && hotword_service->IsServiceAvailable();
}

// static
bool HotwordServiceFactory::IsHotwordAllowed(BrowserContext* context) {
  HotwordService* hotword_service = GetForProfile(context);
  return hotword_service && hotword_service->IsHotwordAllowed();
}

// static
bool HotwordServiceFactory::IsAlwaysOnAvailable() {
#if defined(OS_CHROMEOS)
  if (HotwordService::IsHotwordHardwareAvailable())
    return true;
#endif
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  return command_line->HasSwitch(switches::kEnableExperimentalHotwordHardware);
}

// static
int HotwordServiceFactory::GetCurrentError(BrowserContext* context) {
  HotwordService* hotword_service = GetForProfile(context);
  if (!hotword_service)
    return 0;
  return hotword_service->error_message();
}

HotwordServiceFactory::HotwordServiceFactory()
    : BrowserContextKeyedServiceFactory(
        "HotwordService",
        BrowserContextDependencyManager::GetInstance()) {
  // No dependencies.
}

HotwordServiceFactory::~HotwordServiceFactory() {
}

void HotwordServiceFactory::UpdateMicrophoneState() {
  // In order to trigger the monitor, just call getAudioCaptureDevices.
  content::MediaStreamDevices devices =
    MediaCaptureDevicesDispatcher::GetInstance()->GetAudioCaptureDevices();
}

void HotwordServiceFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* prefs) {
  prefs->RegisterBooleanPref(prefs::kHotwordAudioLoggingEnabled,
                             false,
                             user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  prefs->RegisterStringPref(prefs::kHotwordPreviousLanguage,
                            std::string(),
                            user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  // Per-device settings (do not sync).
  prefs->RegisterBooleanPref(prefs::kHotwordSearchEnabled, false);
  prefs->RegisterBooleanPref(prefs::kHotwordAlwaysOnSearchEnabled, false);
  prefs->RegisterBooleanPref(prefs::kHotwordAlwaysOnNotificationSeen, false);
}

KeyedService* HotwordServiceFactory::BuildServiceInstanceFor(
    BrowserContext* context) const {
  return new HotwordService(Profile::FromBrowserContext(context));
}
