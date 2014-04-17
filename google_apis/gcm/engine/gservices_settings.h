// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_GCM_ENGINE_GSERVICES_SETTINGS_H_
#define GOOGLE_APIS_GCM_ENGINE_GSERVICES_SETTINGS_H_

#include <map>
#include <string>

#include "base/memory/weak_ptr.h"
#include "google_apis/gcm/base/gcm_export.h"
#include "google_apis/gcm/engine/gcm_store.h"
#include "google_apis/gcm/protocol/checkin.pb.h"

namespace gcm {

// Class responsible for handling G-services settings. It takes care of
// extracting them from checkin response and storing in GCMStore.
class GCM_EXPORT GServicesSettings {
 public:
  // Create an instance of GServicesSettings class. |gcm_store| is used to store
  // the settings after they are extracted from checkin response.
  explicit GServicesSettings(GCMStore* gcm_store);
  ~GServicesSettings();

  // Udpates the settings based on |checkin_response|.
  void UpdateFromCheckinResponse(
      const checkin_proto::AndroidCheckinResponse& checkin_response);

  // Updates the settings based on |load_result|.
  void UpdateFromLoadResult(const GCMStore::LoadResult& load_result);

  const std::string& digest() const { return digest_; }

  // TODO(fgorski): Consider returning TimeDelta.
  int64 checkin_interval() const { return checkin_interval_; }

  // TODO(fgorski): Consider returning GURL and use it for validation.
  const std::string& checkin_url() const { return checkin_url_; }

  // TODO(fgorski): Consider returning GURL and use it for validation.
  const std::string& mcs_hostname() const { return mcs_hostname_; }

  int mcs_secure_port() const { return mcs_secure_port_; }

  // TODO(fgorski): Consider returning GURL and use it for validation.
  const std::string& registration_url() const { return registration_url_; }

 private:
  // Parses the |settings| to fill in specific fields.
  // TODO(fgorski): Change to a status variable that can be logged to UMA.
  bool UpdateSettings(const std::map<std::string, std::string>& settings);

  // Callback passed to GCMStore::SetGServicesSettings.
  void SetGServicesSettingsCallback(bool success);

  // GCM store to persist the settings. Not owned.
  GCMStore* gcm_store_;

  // Digest (hash) of the settings, used to check whether settings need update.
  // It is meant to be sent with checkin request, instead of sending the whole
  // settings table.
  std::string digest_;

  // Time in seconds between periodic checkins.
  int64 checkin_interval_;

  // URL that should be used for checkins.
  std::string checkin_url_;

  // Hostname of the MCS server.
  std::string mcs_hostname_;

  // Secure port to connect to on MCS sever.
  int mcs_secure_port_;

  // URL that should be used for regisrations and unregistrations.
  std::string registration_url_;

  // Factory for creating references in callbacks.
  base::WeakPtrFactory<GServicesSettings> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(GServicesSettings);
};

}  // namespace gcm

#endif  // GOOGLE_APIS_GCM_ENGINE_GSERVICES_SETTINGS_H_
