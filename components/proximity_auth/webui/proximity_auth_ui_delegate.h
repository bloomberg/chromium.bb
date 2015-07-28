// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PROXIMITY_AUTH_WEBUI_PROXIMITY_AUTH_UI_DELEGATE_H_
#define COMPONENTS_PROXIMITY_AUTH_WEBUI_PROXIMITY_AUTH_UI_DELEGATE_H_

#include "base/memory/scoped_ptr.h"
#include "components/proximity_auth/cryptauth/proto/cryptauth_api.pb.h"

class PrefService;

namespace gcm {
class GCMDriver;
}

namespace proximity_auth {

class CryptAuthClientFactory;
class SecureMessageDelegate;

// A delegate used by the chrome://proximity-auth WebUI, used to get
// implmentations with dependencies on chrome.
class ProximityAuthUIDelegate {
 public:
  virtual ~ProximityAuthUIDelegate() {}

  // Returns the PrefService used by the profile.
  virtual PrefService* GetPrefService() = 0;

  // Returns the SecureMessageDelegate used by the system.
  virtual scoped_ptr<SecureMessageDelegate> CreateSecureMessageDelegate() = 0;

  // Constructs the CryptAuthClientFactory that can be used for API requests.
  virtual scoped_ptr<CryptAuthClientFactory> CreateCryptAuthClientFactory() = 0;

  // Constructs the DeviceClassifier message that is sent to CryptAuth for all
  // API requests.
  virtual cryptauth::DeviceClassifier GetDeviceClassifier() = 0;

  // Returns the GCMDriver instance used by Chrome.
  virtual gcm::GCMDriver* GetGCMDriver() = 0;
};

}  // namespace proximity_auth

#endif  // COMPONENTS_PROXIMITY_AUTH_WEBUI_PROXIMITY_AUTH_UI_DELEGATE_H_
