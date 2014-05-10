// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gcm/engine/gservices_settings.h"

#include "base/bind.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"

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
const int64 kMinimumCheckinInterval = 12 * 60 * 60;      // seconds = 12 hours.
const char kDefaultCheckinURL[] = "https://android.clients.google.com/checkin";
const char kDefaultMCSHostname[] = "mtalk.google.com";
const int kDefaultMCSMainSecurePort = 5228;
const int kDefaultMCSFallbackSecurePort = 443;
const char kDefaultRegistrationURL[] =
    "https://android.clients.google.com/c2dm/register3";
const char kMCSEnpointTemplate[] = "https://%s:%d";

std::string MakeMCSEndpoint(const std::string& mcs_hostname, int port) {
  return base::StringPrintf(kMCSEnpointTemplate, mcs_hostname.c_str(), port);
}

}  // namespace

namespace gcm {

// static
const base::TimeDelta GServicesSettings::MinimumCheckinInterval() {
  return base::TimeDelta::FromSeconds(kMinimumCheckinInterval);
}

// static
const GURL GServicesSettings::DefaultCheckinURL() {
  return GURL(kDefaultCheckinURL);
}

GServicesSettings::GServicesSettings()
    : checkin_interval_(base::TimeDelta::FromSeconds(kDefaultCheckinInterval)),
      checkin_url_(kDefaultCheckinURL),
      mcs_main_endpoint_(MakeMCSEndpoint(kDefaultMCSHostname,
                                         kDefaultMCSMainSecurePort)),
      mcs_fallback_endpoint_(MakeMCSEndpoint(kDefaultMCSHostname,
                                             kDefaultMCSFallbackSecurePort)),
      registration_url_(kDefaultRegistrationURL),
      weak_ptr_factory_(this) {
}

GServicesSettings::~GServicesSettings() {}

bool GServicesSettings::UpdateFromCheckinResponse(
  const checkin_proto::AndroidCheckinResponse& checkin_response) {
  if (!checkin_response.has_digest() ||
      checkin_response.digest() == digest_) {
    // There are no changes as digest is the same or no settings provided.
    return false;
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
    return true;
  }

  return false;
}

void GServicesSettings::UpdateFromLoadResult(
    const GCMStore::LoadResult& load_result) {
  if (UpdateSettings(load_result.gservices_settings))
    digest_ = load_result.gservices_digest;
}

std::map<std::string, std::string> GServicesSettings::GetSettingsMap() const {
  std::map<std::string, std::string> settings;
  settings[kCheckinIntervalKey] =
      base::Int64ToString(checkin_interval_.InSeconds());
  settings[kCheckinURLKey] = checkin_url_.spec();
  settings[kMCSHostnameKey] = mcs_main_endpoint_.host();
  settings[kMCSSecurePortKey] = mcs_main_endpoint_.port();
  settings[kRegistrationURLKey] = registration_url_.spec();
  return settings;
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
  if (new_checkin_interval == std::numeric_limits<int64>::max()) {
    LOG(ERROR) << "Checkin interval is too big: " << new_checkin_interval;
    return false;
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

  GURL new_mcs_main_endpoint =
      GURL(MakeMCSEndpoint(new_mcs_hostname, new_mcs_secure_port));
  GURL new_mcs_fallback_endpoint =
      GURL(MakeMCSEndpoint(new_mcs_hostname, kDefaultMCSFallbackSecurePort));
  if (!new_mcs_main_endpoint.is_valid() ||
      !new_mcs_fallback_endpoint.is_valid())
    return false;

  GURL new_checkin_url;
  iter = settings.find(kCheckinURLKey);
  if (iter == settings.end()) {
    LOG(ERROR) << "Setting not found: " << kCheckinURLKey;
    return false;
  }
  new_checkin_url = GURL(iter->second);
  if (!new_checkin_url.is_valid()) {
    LOG(ERROR) << "Invalid checkin URL provided: "
               << new_checkin_url.possibly_invalid_spec();
    return false;
  }

  GURL new_registration_url;
  iter = settings.find(kRegistrationURLKey);
  if (iter == settings.end()) {
    LOG(ERROR) << "Setting not found: " << kRegistrationURLKey;
    return false;
  }
  new_registration_url = GURL(iter->second);
  if (!new_registration_url.is_valid()) {
    LOG(ERROR) << "Invalid registration URL provided: "
               << new_registration_url.possibly_invalid_spec();
    return false;
  }

  // We only update the settings once all of them are correct.
  checkin_interval_ = base::TimeDelta::FromSeconds(new_checkin_interval);
  mcs_main_endpoint_ = new_mcs_main_endpoint;
  mcs_fallback_endpoint_ = new_mcs_fallback_endpoint;
  checkin_url_ = new_checkin_url;
  registration_url_ = new_registration_url;
  return true;
}

}  // namespace gcm
