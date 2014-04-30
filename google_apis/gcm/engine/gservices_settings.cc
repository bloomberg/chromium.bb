// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gcm/engine/gservices_settings.h"

#include "base/bind.h"
#include "base/strings/string_number_conversions.h"

namespace {
// The expected time in seconds between periodic checkins.
const char kCheckinIntervalKey[] = "checkin_interval";
// The override URL to the checkin server.
const char kCheckinURLKey[] = "checkin_url";
// The MCS machine name to connect to.
const char kMCSHostnameKey[] = "gcm_hostname";
// The MCS port to connect to.
const char kMCSSecurePortKey[] = "gcm_secure_port";
// The URL to get MCS registration IDs.
const char kRegistrationURLKey[] = "gcm_registration_url";

const int64 kDefaultCheckinInterval = 2 * 24 * 60 * 60;  // seconds = 2 days.
const char kDefaultCheckinURL[] = "https://android.clients.google.com/checkin";
const char kDefaultMCSHostname[] = "https://mtalk.google.com";
const int kDefaultMCSSecurePort = 5228;
const char kDefaultRegistrationURL[] =
    "https://android.clients.google.com/c2dm/register3";

}  // namespace

namespace gcm {

const int64 GServicesSettings::kMinimumCheckinInterval = 12 * 60 * 60;

GServicesSettings::GServicesSettings(GCMStore* gcm_store)
    : gcm_store_(gcm_store),
      checkin_interval_(kDefaultCheckinInterval),
      checkin_url_(kDefaultCheckinURL),
      mcs_hostname_(kDefaultMCSHostname),
      mcs_secure_port_(kDefaultMCSSecurePort),
      registration_url_(kDefaultRegistrationURL),
      weak_ptr_factory_(this) {
}

GServicesSettings::~GServicesSettings() {}

void GServicesSettings::UpdateFromCheckinResponse(
  const checkin_proto::AndroidCheckinResponse& checkin_response) {
  if (!checkin_response.has_digest() ||
      checkin_response.digest() == digest_) {
    // There are no changes as digest is the same or no settings provided.
    return;
  }

  std::map<std::string, std::string> settings;
  for (int i = 0; i < checkin_response.setting_size(); ++i) {
    std::string name = checkin_response.setting(i).name();
    std::string value = checkin_response.setting(i).value();
    settings[name] = value;
  }

  // Only update the settings in store and digest, if the settings actually
  // passed the verificaiton in update settings.
  if (UpdateSettings(settings)) {
    digest_ = checkin_response.digest();
    gcm_store_->SetGServicesSettings(
        settings,
        digest_,
        base::Bind(&GServicesSettings::SetGServicesSettingsCallback,
                   weak_ptr_factory_.GetWeakPtr()));
  }
}

void GServicesSettings::UpdateFromLoadResult(
    const GCMStore::LoadResult& load_result) {
  if (UpdateSettings(load_result.gservices_settings))
    digest_ = load_result.gservices_digest;
}

bool GServicesSettings::UpdateSettings(
    const std::map<std::string, std::string>& settings) {
  int64 new_checkin_interval = kMinimumCheckinInterval;
  std::map<std::string, std::string>::const_iterator iter =
      settings.find(kCheckinIntervalKey);
  if (iter == settings.end()) {
    LOG(ERROR) << "Setting not found: " << kCheckinIntervalKey;
    return false;
  }
  if (!base::StringToInt64(iter->second, &new_checkin_interval)) {
    LOG(ERROR) << "Failed to parse checkin interval: " << iter->second;
    return false;
  }
  if (new_checkin_interval < kMinimumCheckinInterval) {
    LOG(ERROR) << "Checkin interval: " << new_checkin_interval
               << " is less than allowed minimum: " << kMinimumCheckinInterval;
    new_checkin_interval = kMinimumCheckinInterval;
  }

  std::string new_mcs_hostname;
  iter = settings.find(kMCSHostnameKey);
  if (iter == settings.end()) {
    LOG(ERROR) << "Setting not found: " << kMCSHostnameKey;
    return false;
  }
  new_mcs_hostname = iter->second;
  if (new_mcs_hostname.empty()) {
    LOG(ERROR) << "Empty MCS hostname provided.";
    return false;
  }

  int new_mcs_secure_port = -1;
  iter = settings.find(kMCSSecurePortKey);
  if (iter == settings.end()) {
    LOG(ERROR) << "Setting not found: " << kMCSSecurePortKey;
    return false;
  }
  if (!base::StringToInt(iter->second, &new_mcs_secure_port)) {
    LOG(ERROR) << "Failed to parse MCS secure port: " << iter->second;
    return false;
  }
  if (new_mcs_secure_port < 0 || 65535 < new_mcs_secure_port) {
    LOG(ERROR) << "Incorrect port value: " << new_mcs_secure_port;
    return false;
  }

  std::string new_checkin_url;
  iter = settings.find(kCheckinURLKey);
  if (iter == settings.end()) {
    LOG(ERROR) << "Setting not found: " << kCheckinURLKey;
    return false;
  }
  new_checkin_url = iter->second;
  if (new_checkin_url.empty()) {
    LOG(ERROR) << "Empty checkin URL provided.";
    return false;
  }

  std::string new_registration_url;
  iter = settings.find(kRegistrationURLKey);
  if (iter == settings.end()) {
    LOG(ERROR) << "Setting not found: " << kRegistrationURLKey;
    return false;
  }
  new_registration_url = iter->second;
  if (new_registration_url.empty()) {
    LOG(ERROR) << "Empty registration URL provided.";
    return false;
  }

  // We only update the settings once all of them are correct.
  checkin_interval_ = new_checkin_interval;
  mcs_hostname_ = new_mcs_hostname;
  mcs_secure_port_ = new_mcs_secure_port;
  checkin_url_ = new_checkin_url;
  registration_url_ = new_registration_url;
  return true;
}

void GServicesSettings::SetGServicesSettingsCallback(bool success) {
  DCHECK(success);
}

}  // namespace gcm
