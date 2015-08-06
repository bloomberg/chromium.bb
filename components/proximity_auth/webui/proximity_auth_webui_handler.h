// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PROXIMITY_AUTH_WEBUI_PROXIMITY_AUTH_WEBUI_HANDLER_H_
#define COMPONENTS_PROXIMITY_AUTH_WEBUI_PROXIMITY_AUTH_WEBUI_HANDLER_H_

#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "components/proximity_auth/authenticator.h"
#include "components/proximity_auth/client_observer.h"
#include "components/proximity_auth/connection_observer.h"
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

namespace cryptauth {
class ExternalDeviceInfo;
}

namespace proximity_auth {

class Authenticator;
class BluetoothConnection;
class Connection;
class ClientImpl;
class ReachablePhoneFlow;
struct RemoteStatusUpdate;
class SecureContext;

// Handles messages from the chrome://proximity-auth page.
class ProximityAuthWebUIHandler : public content::WebUIMessageHandler,
                                  public LogBuffer::Observer,
                                  public CryptAuthEnrollmentManager::Observer,
                                  public CryptAuthDeviceManager::Observer,
                                  public ConnectionObserver,
                                  public ClientObserver {
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
  void ToggleUnlockKey(const base::ListValue* args);
  void FindEligibleUnlockDevices(const base::ListValue* args);
  void FindReachableDevices(const base::ListValue* args);
  void GetLocalState(const base::ListValue* args);
  void ForceEnrollment(const base::ListValue* args);
  void ForceDeviceSync(const base::ListValue* args);
  void ToggleConnection(const base::ListValue* args);

  // Initializes CryptAuth managers, used for development purposes.
  void InitGCMManager();
  void InitEnrollmentManager();
  void InitDeviceManager();

  // Called when a CryptAuth request fails.
  void OnCryptAuthClientError(const std::string& error_message);

  // Called when the toggleUnlock request succeeds.
  void OnEasyUnlockToggled(const cryptauth::ToggleEasyUnlockResponse& response);

  // Called when the findEligibleUnlockDevices request succeeds.
  void OnFoundEligibleUnlockDevices(
      const cryptauth::FindEligibleUnlockDevicesResponse& response);

  // Callback when |reachable_phone_flow_| completes.
  void OnReachablePhonesFound(
      const std::vector<cryptauth::ExternalDeviceInfo>& reachable_phones);

  // Called when the key agreement of PSK of the remote device completes.
  void OnPSKDerived(const cryptauth::ExternalDeviceInfo& unlock_key,
                    const std::string& persistent_symmetric_key);

  // Callbacks for bluetooth_util::SeekDeviceByAddress().
  void OnSeekedDeviceByAddress();
  void OnSeekedDeviceByAddressError(const std::string& error_message);

  // Callback when |authenticator_| completes authentication.
  void OnAuthenticationResult(Authenticator::Result result,
                              scoped_ptr<SecureContext> secure_context);

  // Creates the client which parses status updates.
  void CreateStatusUpdateClient();

  // Returns the active connection, whether it's owned the |this| instance or
  // |client_|.
  Connection* GetConnection();

  // Converts an ExternalDeviceInfo proto to a JSON dictionary used in
  // JavaScript.
  scoped_ptr<base::DictionaryValue> ExternalDeviceInfoToDictionary(
      const cryptauth::ExternalDeviceInfo& device_info);

  // Converts an IneligibleDevice proto to a JSON dictionary used in JavaScript.
  scoped_ptr<base::DictionaryValue> IneligibleDeviceToDictionary(
      const cryptauth::IneligibleDevice& ineligible_device);

  // ConnectionObserver:
  void OnConnectionStatusChanged(Connection* connection,
                                 Connection::Status old_status,
                                 Connection::Status new_status) override;
  void OnMessageReceived(const Connection& connection,
                         const WireMessage& message) override;

  // ClientObserver:
  void OnRemoteStatusUpdate(const RemoteStatusUpdate& status_update) override;

  // Returns the current enrollment state that can be used as a JSON object.
  scoped_ptr<base::DictionaryValue> GetEnrollmentStateDictionary();

  // Returns the current device sync state that can be used as a JSON object.
  scoped_ptr<base::DictionaryValue> GetDeviceSyncStateDictionary();

  // Returns the current unlock keys that can be used as a JSON object.
  scoped_ptr<base::ListValue> GetUnlockKeysList();

  // The delegate used to fetch dependencies. Must outlive this instance.
  ProximityAuthUIDelegate* delegate_;

  // Creates CryptAuth client instances to make API calls.
  scoped_ptr<CryptAuthClientFactory> cryptauth_client_factory_;

  // We only support one concurrent API call.
  scoped_ptr<CryptAuthClient> cryptauth_client_;

  // The flow for getting a list of reachable phones.
  scoped_ptr<ReachablePhoneFlow> reachable_phone_flow_;

  // True if the WebContents backing the WebUI has been initialized.
  bool web_contents_initialized_;

  // Member variables related to CryptAuth debugging.
  // TODO(tengs): These members are temporarily used for development.
  scoped_ptr<PrefService> pref_service;
  scoped_ptr<CryptAuthGCMManager> gcm_manager_;
  scoped_ptr<CryptAuthEnrollmentManager> enrollment_manager_;
  scoped_ptr<CryptAuthDeviceManager> device_manager_;
  std::string user_public_key_;
  std::string user_private_key_;

  // Member variables for connecting to and authenticating the remote device.
  // TODO(tengs): Support multiple simultaenous connections.
  scoped_ptr<SecureMessageDelegate> secure_message_delegate_;
  scoped_ptr<BluetoothConnection> bluetooth_connection_;
  scoped_ptr<Authenticator> authenticator_;
  scoped_ptr<SecureContext> secure_context_;
  scoped_ptr<ClientImpl> client_;
  scoped_ptr<RemoteStatusUpdate> last_remote_status_update_;

  base::WeakPtrFactory<ProximityAuthWebUIHandler> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ProximityAuthWebUIHandler);
};

}  // namespace proximity_auth

#endif  // COMPONENTS_PROXIMITY_AUTH_WEBUI_PROXIMITY_AUTH_WEBUI_HANDLER_H_
