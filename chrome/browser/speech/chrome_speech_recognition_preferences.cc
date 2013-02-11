// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/speech/chrome_speech_recognition_preferences.h"

#include "base/bind.h"
#include "base/prefs/pref_service.h"
#include "base/values.h"
#include "chrome/browser/prefs/pref_registry_syncable.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_dependency_manager.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_details.h"

using base::ListValue;
using content::BrowserThread;

namespace {
const bool kDefaultFilterProfanities = true;
const bool kDefaultShownSecurityNotification = false;
}

ChromeSpeechRecognitionPreferences::Factory*
ChromeSpeechRecognitionPreferences::Factory::GetInstance() {
  return Singleton<ChromeSpeechRecognitionPreferences::Factory>::get();
}

void ChromeSpeechRecognitionPreferences::InitializeFactory() {
  ChromeSpeechRecognitionPreferences::Factory::GetInstance();
}

scoped_refptr<ChromeSpeechRecognitionPreferences>
ChromeSpeechRecognitionPreferences::Factory::GetForProfile(Profile* profile) {
  DCHECK(profile);
  // GetServiceForProfile will let us instantiate a new (if not already cached
  // for the profile) Service through BuildServiceInstanceFor method.
  ChromeSpeechRecognitionPreferences::Service* service =
      static_cast<ChromeSpeechRecognitionPreferences::Service*>(
          GetServiceForProfile(profile, true));

  if (!service) {
    // Incognito won't have this service.
    return NULL;
  }

  return service->GetPreferences();
}

ChromeSpeechRecognitionPreferences::Factory::Factory()
    : ProfileKeyedServiceFactory(
        "ChromeSpeechRecognitionPreferences",
        ProfileDependencyManager::GetInstance()) {
}

ChromeSpeechRecognitionPreferences::Factory::~Factory() {
}

ProfileKeyedService*
ChromeSpeechRecognitionPreferences::Factory::BuildServiceInstanceFor(
    Profile* profile) const {
  DCHECK(profile);
  return new ChromeSpeechRecognitionPreferences::Service(profile);
}

void ChromeSpeechRecognitionPreferences::Factory::RegisterUserPrefs(
    PrefRegistrySyncable* prefs) {
  prefs->RegisterBooleanPref(
      prefs::kSpeechRecognitionFilterProfanities,
      kDefaultFilterProfanities,
      PrefRegistrySyncable::UNSYNCABLE_PREF);

  prefs->RegisterListPref(
      prefs::kSpeechRecognitionTrayNotificationShownContexts,
      PrefRegistrySyncable::UNSYNCABLE_PREF);
}

bool ChromeSpeechRecognitionPreferences::Factory::
ServiceRedirectedInIncognito() const {
  return false;
}

bool ChromeSpeechRecognitionPreferences::Factory::
ServiceIsNULLWhileTesting() const {
  return true;
}

bool ChromeSpeechRecognitionPreferences::Factory::
ServiceIsCreatedWithProfile() const {
  return false;
}

ChromeSpeechRecognitionPreferences::Service::Service(
    Profile* profile)
    : preferences_(new ChromeSpeechRecognitionPreferences(profile)) {
}

ChromeSpeechRecognitionPreferences::Service::~Service() {
}

void ChromeSpeechRecognitionPreferences::Service::Shutdown() {
  DCHECK(preferences_);
  preferences_->DetachFromProfile();
}

scoped_refptr<ChromeSpeechRecognitionPreferences>
ChromeSpeechRecognitionPreferences::Service::GetPreferences() const {
  return preferences_;
}

scoped_refptr<ChromeSpeechRecognitionPreferences>
ChromeSpeechRecognitionPreferences::GetForProfile(Profile* profile) {
  scoped_refptr<ChromeSpeechRecognitionPreferences> ret;
  if (profile) {
    // Note that when in incognito, GetForProfile will return NULL.
    // We catch that case below and return the default preferences.
    ret = Factory::GetInstance()->GetForProfile(profile);
  }

  if (!ret) {
    // Create a detached preferences object if no profile is provided.
    ret = new ChromeSpeechRecognitionPreferences(NULL);
  }

  return ret;
}

ChromeSpeechRecognitionPreferences::ChromeSpeechRecognitionPreferences(
    Profile* profile)
    : profile_(profile),
      pref_change_registrar_(new PrefChangeRegistrar()),
      filter_profanities_(kDefaultFilterProfanities),
      notifications_shown_(new ListValue()) {
  if (!profile_)
    return;

  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  pref_change_registrar_->Init(profile_->GetPrefs());

  ReloadFilterProfanities();
  pref_change_registrar_->Add(
      prefs::kSpeechRecognitionFilterProfanities,
      base::Bind(&ChromeSpeechRecognitionPreferences::ReloadFilterProfanities,
                 base::Unretained(this)));

  ReloadNotificationsShown();
  pref_change_registrar_->Add(
      prefs::kSpeechRecognitionTrayNotificationShownContexts,
      base::Bind(&ChromeSpeechRecognitionPreferences::ReloadNotificationsShown,
                 base::Unretained(this)));
}

ChromeSpeechRecognitionPreferences::~ChromeSpeechRecognitionPreferences() {
}

void ChromeSpeechRecognitionPreferences::DetachFromProfile() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(profile_);
  pref_change_registrar_.reset();
  profile_ = NULL;
}

bool ChromeSpeechRecognitionPreferences::FilterProfanities() const {
  base::AutoLock read_lock(preferences_lock_);
  return filter_profanities_;
}

void ChromeSpeechRecognitionPreferences::SetFilterProfanities(
    bool filter_profanities) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  {
    base::AutoLock write_lock(preferences_lock_);
    filter_profanities_ = filter_profanities;
  }
  if (profile_) {
    profile_->GetPrefs()->SetBoolean(prefs::kSpeechRecognitionFilterProfanities,
                                     filter_profanities);
  }
}

void ChromeSpeechRecognitionPreferences::ToggleFilterProfanities() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  SetFilterProfanities(!FilterProfanities());
}

bool ChromeSpeechRecognitionPreferences::ShouldShowSecurityNotification(
    const std::string& context_name) const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(notifications_shown_.get());
  scoped_ptr<base::StringValue> match_name(
      base::Value::CreateStringValue(context_name));
  return notifications_shown_->Find(*match_name) == notifications_shown_->end();
}

void ChromeSpeechRecognitionPreferences::SetHasShownSecurityNotification(
    const std::string& context_name) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(notifications_shown_.get());
  notifications_shown_->AppendIfNotPresent(
      base::Value::CreateStringValue(context_name));
  if (profile_) {
    profile_->GetPrefs()->Set(
        prefs::kSpeechRecognitionTrayNotificationShownContexts,
        *notifications_shown_);
  }
}

void ChromeSpeechRecognitionPreferences::ReloadFilterProfanities() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  base::AutoLock write_lock(preferences_lock_);
  filter_profanities_ = profile_->GetPrefs()->GetBoolean(
      prefs::kSpeechRecognitionFilterProfanities);
}

void ChromeSpeechRecognitionPreferences::ReloadNotificationsShown() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  base::AutoLock write_lock(preferences_lock_);
  const base::ListValue* pref_list = profile_->GetPrefs()->GetList(
      prefs::kSpeechRecognitionTrayNotificationShownContexts);
  notifications_shown_.reset(pref_list->DeepCopy());
}
