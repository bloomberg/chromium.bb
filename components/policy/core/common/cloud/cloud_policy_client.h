// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_CLOUD_CLOUD_POLICY_CLIENT_H_
#define COMPONENTS_POLICY_CORE_COMMON_CLOUD_CLOUD_POLICY_CLIENT_H_

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/policy_export.h"
#include "policy/proto/device_management_backend.pb.h"

namespace net {
class URLRequestContextGetter;
}

namespace policy {

class DeviceManagementRequestJob;
class DeviceManagementService;

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
  // Maps a PolicyNamespaceKey to its corresponding PolicyFetchResponse.
  typedef std::map<PolicyNamespaceKey,
                   enterprise_management::PolicyFetchResponse*> ResponseMap;

  // A callback which receives boolean status of an operation.  If the operation
  // succeeded, |status| is true.
  typedef base::Callback<void(bool status)> StatusCallback;

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

  // Delegate interface for supplying status information to upload to the server
  // as part of the policy fetch request.
  class POLICY_EXPORT StatusProvider {
   public:
    virtual ~StatusProvider();

    // Retrieves status information to send with the next policy fetch.
    // Implementations must return true if status information was filled in.
    virtual bool GetDeviceStatus(
        enterprise_management::DeviceStatusReportRequest* status) = 0;
    virtual bool GetSessionStatus(
        enterprise_management::SessionStatusReportRequest* status) = 0;

    // Called after the status information has successfully been submitted to
    // the server.
    virtual void OnSubmittedSuccessfully() = 0;
  };

  // |provider| and |service| are weak pointers and it's the caller's
  // responsibility to keep them valid for the lifetime of CloudPolicyClient.
  // |verification_key_hash| contains an identifier telling the DMServer which
  // verification key to use.
  CloudPolicyClient(
      const std::string& machine_id,
      const std::string& machine_model,
      const std::string& verification_key_hash,
      UserAffiliation user_affiliation,
      StatusProvider* provider,
      DeviceManagementService* service,
      scoped_refptr<net::URLRequestContextGetter> request_context);
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
      const std::string& auth_token,
      const std::string& client_id,
      bool is_auto_enrollment,
      const std::string& requisition,
      const std::string& current_state_key);

  // Sets information about a policy invalidation. Subsequent fetch operations
  // will use the given info, and callers can use fetched_invalidation_version
  // to determine which version of policy was fetched.
  void SetInvalidationInfo(int64 version, const std::string& payload);

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

  // Upload a device certificate to the server.  Like FetchPolicy, this method
  // requires that the client is in a registered state.  |certificate_data| must
  // hold the X.509 certificate data to be sent to the server.  The |callback|
  // will be called when the operation completes.
  virtual void UploadCertificate(const std::string& certificate_data,
                                 const StatusCallback& callback);

  // Adds an observer to be called back upon policy and state changes.
  void AddObserver(Observer* observer);

  // Removes the specified observer.
  void RemoveObserver(Observer* observer);

  void set_submit_machine_id(bool submit_machine_id) {
    submit_machine_id_ = submit_machine_id;
  }

  void set_last_policy_timestamp(const base::Time& timestamp) {
    last_policy_timestamp_ = timestamp;
  }

  void set_public_key_version(int public_key_version) {
    public_key_version_ = public_key_version;
    public_key_version_valid_ = true;
  }

  void clear_public_key_version() {
    public_key_version_valid_ = false;
  }

  // FetchPolicy() calls will request this policy namespace.
  void AddNamespaceToFetch(const PolicyNamespaceKey& policy_ns_key);

  // FetchPolicy() calls won't request the given policy namespace anymore.
  void RemoveNamespaceToFetch(const PolicyNamespaceKey& policy_ns_key);

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

  // Returns the policy response for |policy_ns_key|, if found in |responses()|;
  // otherwise returns NULL.
  const enterprise_management::PolicyFetchResponse* GetPolicyFor(
      const PolicyNamespaceKey& policy_ns_key) const;

  DeviceManagementStatus status() const {
    return status_;
  }

  const std::string& robot_api_auth_code() const {
    return robot_api_auth_code_;
  }

  // Returns the invalidation version that was used for the last FetchPolicy.
  // Observers can call this method from their OnPolicyFetched method to
  // determine which at which invalidation version the policy was fetched.
  int64 fetched_invalidation_version() const {
    return fetched_invalidation_version_;
  }

  scoped_refptr<net::URLRequestContextGetter> GetRequestContext();

 protected:
  // A set of PolicyNamespaceKeys to fetch.
  typedef std::set<PolicyNamespaceKey> NamespaceSet;

  // Callback for retries of registration requests.
  void OnRetryRegister(DeviceManagementRequestJob* job);

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
      const StatusCallback& callback,
      DeviceManagementStatus status,
      int net_error,
      const enterprise_management::DeviceManagementResponse& response);

  // Observer notification helpers.
  void NotifyPolicyFetched();
  void NotifyRegistrationStateChanged();
  void NotifyRobotAuthCodesFetched();
  void NotifyClientError();

  // Data necessary for constructing policy requests.
  const std::string machine_id_;
  const std::string machine_model_;
  const std::string verification_key_hash_;
  const UserAffiliation user_affiliation_;
  NamespaceSet namespaces_to_fetch_;
  std::vector<std::string> state_keys_to_upload_;

  std::string dm_token_;
  DeviceMode device_mode_;
  std::string client_id_;
  bool submit_machine_id_;
  base::Time last_policy_timestamp_;
  int public_key_version_;
  bool public_key_version_valid_;
  std::string robot_api_auth_code_;

  // Information for the latest policy invalidation received.
  int64 invalidation_version_;
  std::string invalidation_payload_;

  // The invalidation version used for the most recent fetch operation.
  int64 fetched_invalidation_version_;

  // Used for issuing requests to the cloud.
  DeviceManagementService* service_;
  scoped_ptr<DeviceManagementRequestJob> request_job_;

  // Status upload data is produced by |status_provider_|.
  StatusProvider* status_provider_;

  // The policy responses returned by the last policy fetch operation.
  ResponseMap responses_;
  DeviceManagementStatus status_;

  ObserverList<Observer, true> observers_;
  scoped_refptr<net::URLRequestContextGetter> request_context_;

 private:
  DISALLOW_COPY_AND_ASSIGN(CloudPolicyClient);
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_CLOUD_CLOUD_POLICY_CLIENT_H_
