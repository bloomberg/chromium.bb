// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_CLOUD_CLOUD_POLICY_CLIENT_H_
#define COMPONENTS_POLICY_CORE_COMMON_CLOUD_CLOUD_POLICY_CLIENT_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/remote_commands/remote_command_job.h"
#include "components/policy/policy_export.h"
#include "components/policy/proto/device_management_backend.pb.h"

namespace net {
class URLRequestContextGetter;
}

namespace policy {

class DeviceManagementRequestJob;
class DeviceManagementService;
class SigningService;

// Implements the core logic required to talk to the device management service.
// Also keeps track of the current state of the association with the service,
// such as whether there is a valid registration (DMToken is present in that
// case) and whether and what errors occurred in the latest request.
//
// Note that CloudPolicyClient doesn't do any validation of policy responses
// such as signature and time stamp checks. These happen once the policy gets
// installed in the cloud policy cache.
class POLICY_EXPORT CloudPolicyClient {
 public:
  // Maps a (policy type, settings entity ID) pair to its corresponding
  // PolicyFetchResponse.
  using ResponseMap =
      std::map<std::pair<std::string, std::string>,
               std::unique_ptr<enterprise_management::PolicyFetchResponse>>;

  // Maps a license type to number of available licenses.
  using LicenseMap = std::map<LicenseType, int>;

  // A callback which receives boolean status of an operation.  If the operation
  // succeeded, |status| is true.
  using StatusCallback = base::Callback<void(bool status)>;

  // A callback for available licenses request. If the operation succeeded,
  // |success| is true, and |map| contains available licenses.
  using LicenseRequestCallback =
      base::Callback<void(bool success, const LicenseMap& map)>;

  // A callback which receives fetched remote commands.
  using RemoteCommandCallback = base::Callback<void(
      DeviceManagementStatus,
      const std::vector<enterprise_management::RemoteCommand>&)>;

  // Observer interface for state and policy changes.
  class POLICY_EXPORT Observer {
   public:
    virtual ~Observer();

    // Called when a policy fetch completes successfully. If a policy fetch
    // triggers an error, OnClientError() will fire.
    virtual void OnPolicyFetched(CloudPolicyClient* client) = 0;

    // Called upon registration state changes. This callback is invoked for
    // successful completion of registration and unregistration requests.
    virtual void OnRegistrationStateChanged(CloudPolicyClient* client) = 0;

    // Called when a request for device robot OAuth2 authorization tokens
    // returns successfully. Only occurs during enrollment. Optional
    // (default implementation is a noop).
    virtual void OnRobotAuthCodesFetched(CloudPolicyClient* client);

    // Indicates there's been an error in a previously-issued request.
    virtual void OnClientError(CloudPolicyClient* client) = 0;
  };

  // If non-empty, |machine_id| and |machine_model| are passed to the server
  // verbatim. As these reveal machine identity, they must only be used where
  // this is appropriate (i.e. device policy, but not user policy). |service|
  // and |signing_service| are weak pointers and it's the caller's
  // responsibility to keep them valid for the lifetime of CloudPolicyClient.
  // The |signing_service| is used to sign sensitive requests.
  CloudPolicyClient(
      const std::string& machine_id,
      const std::string& machine_model,
      DeviceManagementService* service,
      scoped_refptr<net::URLRequestContextGetter> request_context,
      SigningService* signing_service);
  virtual ~CloudPolicyClient();

  // Sets the DMToken, thereby establishing a registration with the server. A
  // policy fetch is not automatically issued but can be requested by calling
  // FetchPolicy().
  virtual void SetupRegistration(const std::string& dm_token,
                                 const std::string& client_id);

  // Attempts to register with the device management service. Results in a
  // registration change or error notification.
  virtual void Register(
      enterprise_management::DeviceRegisterRequest::Type registration_type,
      enterprise_management::DeviceRegisterRequest::Flavor flavor,
      enterprise_management::LicenseType::LicenseTypeEnum license_type,
      const std::string& auth_token,
      const std::string& client_id,
      const std::string& requisition,
      const std::string& current_state_key);

  // Attempts to register with the device management service using a
  // registration certificate. Results in a registration change or
  // error notification.
  virtual void RegisterWithCertificate(
      enterprise_management::DeviceRegisterRequest::Type registration_type,
      enterprise_management::DeviceRegisterRequest::Flavor flavor,
      enterprise_management::LicenseType::LicenseTypeEnum license_type,
      const std::string& pem_certificate_chain,
      const std::string& client_id,
      const std::string& requisition,
      const std::string& current_state_key);

  // Sets information about a policy invalidation. Subsequent fetch operations
  // will use the given info, and callers can use fetched_invalidation_version
  // to determine which version of policy was fetched.
  void SetInvalidationInfo(int64_t version, const std::string& payload);

  // Requests a policy fetch. The client being registered is a prerequisite to
  // this operation and this call will CHECK if the client is not in registered
  // state. FetchPolicy() triggers a policy fetch from the cloud. A policy
  // change notification is reported to the observers and the new policy blob
  // can be retrieved once the policy fetch operation completes. In case of
  // multiple requests to fetch policy, new requests will cancel any pending
  // requests and the latest request will eventually trigger notifications.
  virtual void FetchPolicy();

  // Requests OAuth2 auth codes for the device robot account. The client being
  // registered is a prerequisite to this operation and this call will CHECK if
  // the client is not in registered state.
  virtual void FetchRobotAuthCodes(const std::string& auth_token);

  // Sends an unregistration request to the server.
  virtual void Unregister();

  // Upload a machine certificate to the server.  Like FetchPolicy, this method
  // requires that the client is in a registered state.  |certificate_data| must
  // hold the X.509 certificate data to be sent to the server.  The |callback|
  // will be called when the operation completes.
  virtual void UploadEnterpriseMachineCertificate(
      const std::string& certificate_data,
      const StatusCallback& callback);

  // Upload an enrollment certificate to the server.  Like FetchPolicy, this
  // method requires that the client is in a registered state.
  // |certificate_data| must hold the X.509 certificate data to be sent to the
  // server.  The |callback| will be called when the operation completes.
  virtual void UploadEnterpriseEnrollmentCertificate(
      const std::string& certificate_data,
      const StatusCallback& callback);

  // Uploads device/session status to the server. As above, the client must be
  // in a registered state. If non-null, |device_status| and |session_status|
  // will be included in the upload status request. The |callback| will be
  // called when the operation completes.
  virtual void UploadDeviceStatus(
      const enterprise_management::DeviceStatusReportRequest* device_status,
      const enterprise_management::SessionStatusReportRequest* session_status,
      const StatusCallback& callback);

  // Attempts to fetch remote commands, with |last_command_id| being the ID of
  // the last command that finished execution and |command_results| being
  // results for previous commands which have not been reported yet. The
  // |callback| will be called when the operation completes.
  // Note that sending |last_command_id| will acknowledge this command and any
  // previous commands. A nullptr indicates that no commands have finished
  // execution.
  virtual void FetchRemoteCommands(
      std::unique_ptr<RemoteCommandJob::UniqueIDType> last_command_id,
      const std::vector<enterprise_management::RemoteCommandResult>&
          command_results,
      const RemoteCommandCallback& callback);

  // Sends a device attribute update permission request to the server, uses
  // OAuth2 token |auth_token| to identify user who requests a permission to
  // name a device, calls a |callback| from the enrollment screen to indicate
  // whether the device naming prompt should be shown.
  void GetDeviceAttributeUpdatePermission(const std::string& auth_token,
                                          const StatusCallback& callback);

  // Sends a device naming information (Asset Id and Location) to the
  // device management server, uses OAuth2 token |auth_token| to identify user
  // who names a device, the |callback| will be called when the operation
  // completes.
  void UpdateDeviceAttributes(const std::string& auth_token,
                              const std::string& asset_id,
                              const std::string& location,
                              const StatusCallback& callback);

  // Requests a list of licenses available for enrollment. Uses OAuth2 token
  // |auth_token| to identify user who issues the request, the |callback| will
  // be called when the operation completes.
  void RequestAvailableLicenses(const std::string& auth_token,
                                const LicenseRequestCallback& callback);

  // Sends a GCM id update request to the DM server. The server will
  // associate the DM token in authorization header with |gcm_id|, and
  // |callback| will be called when the operation completes.
  virtual void UpdateGcmId(const std::string& gcm_id,
                           const StatusCallback& callback);

  // Adds an observer to be called back upon policy and state changes.
  void AddObserver(Observer* observer);

  // Removes the specified observer.
  void RemoveObserver(Observer* observer);

  const std::string& machine_id() const { return machine_id_; }
  const std::string& machine_model() const { return machine_model_; }

  void set_last_policy_timestamp(const base::Time& timestamp) {
    last_policy_timestamp_ = timestamp;
  }

  const base::Time& last_policy_timestamp() { return last_policy_timestamp_; }

  void set_public_key_version(int public_key_version) {
    public_key_version_ = public_key_version;
    public_key_version_valid_ = true;
  }

  void clear_public_key_version() {
    public_key_version_valid_ = false;
  }

  // FetchPolicy() calls will request this policy type.
  // If |settings_entity_id| is empty then it won't be set in the
  // PolicyFetchRequest.
  void AddPolicyTypeToFetch(const std::string& policy_type,
                            const std::string& settings_entity_id);

  // FetchPolicy() calls won't request the given policy type and optional
  // |settings_entity_id| anymore.
  void RemovePolicyTypeToFetch(const std::string& policy_type,
                               const std::string& settings_entity_id);

  // Configures a set of device state keys to transfer to the server in the next
  // policy fetch. If the fetch is successful, the keys will be cleared so they
  // are only uploaded once.
  void SetStateKeysToUpload(const std::vector<std::string>& keys);

  // Whether the client is registered with the device management service.
  bool is_registered() const { return !dm_token_.empty(); }

  const std::string& dm_token() const { return dm_token_; }
  const std::string& client_id() const { return client_id_; }

  // The device mode as received in the registration request.
  DeviceMode device_mode() const { return device_mode_; }

  // The policy responses as obtained by the last request to the cloud. These
  // policies haven't gone through verification, so their contents cannot be
  // trusted. Use CloudPolicyStore::policy() and CloudPolicyStore::policy_map()
  // instead for making policy decisions.
  const ResponseMap& responses() const {
    return responses_;
  }

  // Returns the policy response for the (|policy_type|, |settings_entity_id|)
  // pair if found in |responses()|. Otherwise returns nullptr.
  const enterprise_management::PolicyFetchResponse* GetPolicyFor(
      const std::string& policy_type,
      const std::string& settings_entity_id) const;

  DeviceManagementStatus status() const {
    return status_;
  }

  const std::string& robot_api_auth_code() const {
    return robot_api_auth_code_;
  }

  // Returns the invalidation version that was used for the last FetchPolicy.
  // Observers can call this method from their OnPolicyFetched method to
  // determine which at which invalidation version the policy was fetched.
  int64_t fetched_invalidation_version() const {
    return fetched_invalidation_version_;
  }

  scoped_refptr<net::URLRequestContextGetter> GetRequestContext();

  // Returns the number of active requests.
  int GetActiveRequestCountForTest() const;

 protected:
  // A set of (policy type, settings entity ID) pairs to fetch.
  typedef std::set<std::pair<std::string, std::string>> PolicyTypeSet;

  // Upload a certificate to the server.  Like FetchPolicy, this method
  // requires that the client is in a registered state.  |certificate_data| must
  // hold the X.509 certificate data to be sent to the server.  The |callback|
  // will be called when the operation completes.
  void UploadCertificate(
      const std::string& certificate_data,
      enterprise_management::DeviceCertUploadRequest::CertificateType
          certificate_type,
      const StatusCallback& callback);

  // Callback for retries of registration requests.
  void OnRetryRegister(DeviceManagementRequestJob* job);

  // Callback for siganture of requests.
  void OnRegisterWithCertificateRequestSigned(bool success,
      enterprise_management::SignedData signed_data);

  // Callback for registration requests.
  void OnRegisterCompleted(
      DeviceManagementStatus status,
      int net_error,
      const enterprise_management::DeviceManagementResponse& response);

  // Callback for policy fetch requests.
  void OnPolicyFetchCompleted(
      DeviceManagementStatus status,
      int net_error,
      const enterprise_management::DeviceManagementResponse& response);

  // Callback for robot account api authorization requests.
  void OnFetchRobotAuthCodesCompleted(
      DeviceManagementStatus status,
      int net_error,
      const enterprise_management::DeviceManagementResponse& response);

  // Callback for unregistration requests.
  void OnUnregisterCompleted(
      DeviceManagementStatus status,
      int net_error,
      const enterprise_management::DeviceManagementResponse& response);

  // Callback for certificate upload requests.
  void OnCertificateUploadCompleted(
      const DeviceManagementRequestJob* job,
      const StatusCallback& callback,
      DeviceManagementStatus status,
      int net_error,
      const enterprise_management::DeviceManagementResponse& response);

  // Callback for status upload requests.
  void OnStatusUploadCompleted(
      const DeviceManagementRequestJob* job,
      const StatusCallback& callback,
      DeviceManagementStatus status,
      int net_error,
      const enterprise_management::DeviceManagementResponse& response);

  // Callback for remote command fetch requests.
  void OnRemoteCommandsFetched(
      const DeviceManagementRequestJob* job,
      const RemoteCommandCallback& callback,
      DeviceManagementStatus status,
      int net_error,
      const enterprise_management::DeviceManagementResponse& response);

  // Callback for device attribute update permission requests.
  void OnDeviceAttributeUpdatePermissionCompleted(
      const DeviceManagementRequestJob* job,
      const StatusCallback& callback,
      DeviceManagementStatus status,
      int net_error,
      const enterprise_management::DeviceManagementResponse& response);

  // Callback for device attribute update requests.
  void OnDeviceAttributeUpdated(
      const DeviceManagementRequestJob* job,
      const StatusCallback& callback,
      DeviceManagementStatus status,
      int net_error,
      const enterprise_management::DeviceManagementResponse& response);

  // Callback for available license types request.
  void OnAvailableLicensesRequested(
      const DeviceManagementRequestJob* job,
      const LicenseRequestCallback& callback,
      DeviceManagementStatus status,
      int net_error,
      const enterprise_management::DeviceManagementResponse& response);

  // Callback for gcm id update requests.
  void OnGcmIdUpdated(
      const DeviceManagementRequestJob* job,
      const StatusCallback& callback,
      DeviceManagementStatus status,
      int net_error,
      const enterprise_management::DeviceManagementResponse& response);

  // Helper to remove a job from request_jobs_.
  void RemoveJob(const DeviceManagementRequestJob* job);

  // Observer notification helpers.
  void NotifyPolicyFetched();
  void NotifyRegistrationStateChanged();
  void NotifyRobotAuthCodesFetched();
  void NotifyClientError();

  // Data necessary for constructing policy requests.
  const std::string machine_id_;
  const std::string machine_model_;
  PolicyTypeSet types_to_fetch_;
  std::vector<std::string> state_keys_to_upload_;

  std::string dm_token_;
  DeviceMode device_mode_ = DEVICE_MODE_NOT_SET;
  std::string client_id_;
  base::Time last_policy_timestamp_;
  int public_key_version_ = -1;
  bool public_key_version_valid_ = false;
  std::string robot_api_auth_code_;

  // Information for the latest policy invalidation received.
  int64_t invalidation_version_ = 0;
  std::string invalidation_payload_;

  // The invalidation version used for the most recent fetch operation.
  int64_t fetched_invalidation_version_ = 0;

  // Used for issuing requests to the cloud.
  DeviceManagementService* service_ = nullptr;

  // Used for signing requests.
  SigningService* signing_service_ = nullptr;

  // Only one outstanding policy fetch is allowed, so this is tracked in
  // its own member variable.
  std::unique_ptr<DeviceManagementRequestJob> policy_fetch_request_job_;

  // All of the outstanding non-policy-fetch request jobs. These jobs are
  // silently cancelled if Unregister() is called.
  std::vector<std::unique_ptr<DeviceManagementRequestJob>> request_jobs_;

  // The policy responses returned by the last policy fetch operation.
  ResponseMap responses_;
  DeviceManagementStatus status_ = DM_STATUS_SUCCESS;

  base::ObserverList<Observer, true> observers_;
  scoped_refptr<net::URLRequestContextGetter> request_context_;

 private:
  void SetClientId(const std::string& client_id);

  // Used to create tasks which run delayed on the UI thread.
  base::WeakPtrFactory<CloudPolicyClient> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(CloudPolicyClient);
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_CLOUD_CLOUD_POLICY_CLIENT_H_
