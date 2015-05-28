// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PROXIMITY_AUTH_WEBUI_PROXIMITY_AUTH_UI_DELEGATE_H_
#define COMPONENTS_PROXIMITY_AUTH_WEBUI_PROXIMITY_AUTH_UI_DELEGATE_H_

#include "base/memory/scoped_ptr.h"
#include "components/proximity_auth/cryptauth/proto/cryptauth_api.pb.h"

namespace proximity_auth {

class CryptAuthClientFactory;

// A delegate used by the chrome://proximity-auth WebUI, used to get
// implmentations with dependencies on chrome.
class ProximityAuthUIDelegate {
 public:
  virtual ~ProximityAuthUIDelegate() {}

  // Constructs the CryptAuthClientFactory that can be used for API requests.
  virtual scoped_ptr<CryptAuthClientFactory> CreateCryptAuthClientFactory() = 0;

  // Constructs the DeviceClassifier message that is sent to CryptAuth for all
  // API requests.
  virtual cryptauth::DeviceClassifier GetDeviceClassifier() = 0;
};

}  // namespace proximity_auth

#endif  // COMPONENTS_PROXIMITY_AUTH_WEBUI_PROXIMITY_AUTH_UI_DELEGATE_H_
