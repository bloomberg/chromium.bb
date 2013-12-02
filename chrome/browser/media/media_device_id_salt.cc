// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/media/media_device_id_salt.h"

#include "base/base64.h"
#include "base/prefs/pref_service.h"
#include "base/rand_util.h"
#include "chrome/browser/profiles/profile_io_data.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace {

std::string CreateSalt() {
  std::string salt;
  base::Base64Encode(base::RandBytesAsString(16), &salt);
  DCHECK(!salt.empty());
  return salt;
}

}  // namespace

MediaDeviceIDSalt::MediaDeviceIDSalt(PrefService* pref_service,
                                     bool incognito) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (incognito) {
    incognito_salt_ = CreateSalt();
    return;
  }

  media_device_id_salt_.Init(prefs::kMediaDeviceIdSalt, pref_service);
  if (media_device_id_salt_.GetValue().empty())
    media_device_id_salt_.SetValue(CreateSalt());

  media_device_id_salt_.MoveToThread(
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::IO));
}

MediaDeviceIDSalt::~MediaDeviceIDSalt() {
}

void MediaDeviceIDSalt::ShutdownOnUIThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (incognito_salt_.empty())
    media_device_id_salt_.Destroy();
}

std::string MediaDeviceIDSalt::GetSalt() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (incognito_salt_.size())
    return incognito_salt_;
  return media_device_id_salt_.GetValue();
}

void MediaDeviceIDSalt::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterStringPref(
      prefs::kMediaDeviceIdSalt,
      std::string(),
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
}

void MediaDeviceIDSalt::Reset(PrefService* pref_service) {
  pref_service->SetString(prefs::kMediaDeviceIdSalt,
                          CreateSalt());
}
