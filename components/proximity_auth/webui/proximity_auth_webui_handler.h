// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PROXIMITY_AUTH_WEBUI_PROXIMITY_AUTH_WEBUI_HANDLER_H_
#define COMPONENTS_PROXIMITY_AUTH_WEBUI_PROXIMITY_AUTH_WEBUI_HANDLER_H_

#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "components/proximity_auth/cryptauth/cryptauth_client.h"
#include "components/proximity_auth/cryptauth/cryptauth_device_manager.h"
#include "components/proximity_auth/cryptauth/cryptauth_enrollment_manager.h"
#include "components/proximity_auth/cryptauth/cryptauth_gcm_manager.h"
#include "components/proximity_auth/logging/log_buffer.h"
#include "components/proximity_auth/webui/proximity_auth_ui_delegate.h"
#include "content/public/browser/web_ui_message_handler.h"

namespace base {
class ListValue;
}

namespace proximity_auth {

// Handles messages from the chrome://proximity-auth page.
class ProximityAuthWebUIHandler : public content::WebUIMessageHandler,
                                  public LogBuffer::Observer,
                                  public CryptAuthEnrollmentManager::Observer,
                                  public CryptAuthDeviceManager::Observer {
 public:
  // |delegate| is not owned and must outlive this instance.
  explicit ProximityAuthWebUIHandler(ProximityAuthUIDelegate* delegate);
  ~ProximityAuthWebUIHandler() override;

  // content::WebUIMessageHandler:
  void RegisterMessages() override;

 private:
  // LogBuffer::Observer:
  void OnLogMessageAdded(const LogBuffer::LogMessage& log_message) override;
  void OnLogBufferCleared() override;

  // CryptAuthEnrollmentManager::Observer:
  void OnEnrollmentStarted() override;
  void OnEnrollmentFinished(bool success) override;

  // CryptAuthDeviceManager::Observer:
  void OnSyncStarted() override;
  void OnSyncFinished(
      CryptAuthDeviceManager::SyncResult sync_result,
      CryptAuthDeviceManager::DeviceChangeResult device_change_result) override;

  // Message handler callbacks.
  void GetLogMessages(const base::ListValue* args);
  void ClearLogBuffer(const base::ListValue* args);
  void FindEligibleUnlockDevices(const base::ListValue* args);
  void GetSyncStates(const base::ListValue* args);
  void ForceEnrollment(const base::ListValue* args);
  void ForceDeviceSync(const base::ListValue* args);

  // Initializes CryptAuth managers, used for development purposes.
  void InitGCMManager();
  void InitEnrollmentManager();
  void InitDeviceManager();

  // Called when a CryptAuth request fails.
  void OnCryptAuthClientError(const std::string& error_message);

  // Called when the findEligibleUnlockDevices request succeeds.
  void OnFoundEligibleUnlockDevices(
      const cryptauth::FindEligibleUnlockDevicesResponse& response);

  // Returns the current enrollment state that can be used as a JSON object.
  scoped_ptr<base::DictionaryValue> GetEnrollmentStateDictionary();

  // Returns the current device sync state that can be used as a JSON object.
  scoped_ptr<base::DictionaryValue> GetDeviceSyncStateDictionary();

  // The delegate used to fetch dependencies. Must outlive this instance.
  ProximityAuthUIDelegate* delegate_;

  // Creates CryptAuth client instances to make API calls.
  scoped_ptr<CryptAuthClientFactory> cryptauth_client_factory_;

  // We only support one concurrent API call.
  scoped_ptr<CryptAuthClient> cryptauth_client_;

  // TODO(tengs): These members are temporarily used for development.
  scoped_ptr<PrefService> pref_service;
  scoped_ptr<CryptAuthGCMManager> gcm_manager_;
  scoped_ptr<CryptAuthEnrollmentManager> enrollment_manager_;
  scoped_ptr<CryptAuthDeviceManager> device_manager_;

  base::WeakPtrFactory<ProximityAuthWebUIHandler> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ProximityAuthWebUIHandler);
};

}  // namespace proximity_auth

#endif  // COMPONENTS_PROXIMITY_AUTH_WEBUI_PROXIMITY_AUTH_WEBUI_HANDLER_H_
