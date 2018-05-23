// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_AUTO_ENROLLMENT_CLIENT_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_AUTO_ENROLLMENT_CLIENT_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "net/base/network_change_notifier.h"
#include "third_party/protobuf/src/google/protobuf/repeated_field.h"

class PrefRegistrySimple;
class PrefService;

namespace enterprise_management {
class DeviceManagementResponse;
}

namespace net {
class URLRequestContextGetter;
}

namespace policy {

class DeviceManagementService;
class DeviceManagementRequestJob;

// Indicates the current state of the auto-enrollment check.  (Numeric values
// are just to make reading of log files easier.)
enum AutoEnrollmentState {
  // Not yet started.
  AUTO_ENROLLMENT_STATE_IDLE = 0,
  // Working, another event will be fired eventually.
  AUTO_ENROLLMENT_STATE_PENDING = 1,
  // Failed to connect to DMServer.
  AUTO_ENROLLMENT_STATE_CONNECTION_ERROR = 2,
  // Connection successful, but the server failed to generate a valid reply.
  AUTO_ENROLLMENT_STATE_SERVER_ERROR = 3,
  // Check completed successfully, enrollment should be triggered.
  AUTO_ENROLLMENT_STATE_TRIGGER_ENROLLMENT = 4,
  // Check completed successfully, enrollment not applicable.
  AUTO_ENROLLMENT_STATE_NO_ENROLLMENT = 5,
  // Check completed successfully, zero-touch enrollment should be triggered.
  AUTO_ENROLLMENT_STATE_TRIGGER_ZERO_TOUCH = 6,
};

// Interacts with the device management service and determines whether this
// machine should automatically enter the Enterprise Enrollment screen during
// OOBE.
class AutoEnrollmentClient
    : public net::NetworkChangeNotifier::NetworkChangeObserver {
 public:
  // The modulus value is sent in an int64_t field in the protobuf, whose
  // maximum value is 2^63-1. So 2^64 and 2^63 can't be represented as moduli
  // and the max is 2^62 (when the moduli are restricted to powers-of-2).
  static const int kMaximumPower = 62;

  // Subclasses of this class provide an identifier and specify the identifier
  // set for the DeviceAutoEnrollmentRequest,
  class DeviceIdentifierProvider;

  // Subclasses of this class generate the request to download the device state
  // (after determining that there is server-side device state) and parse the
  // response.
  class StateDownloadMessageProcessor;

  // Used for signaling progress to a consumer.
  typedef base::RepeatingCallback<void(AutoEnrollmentState)> ProgressCallback;

  // |progress_callback| will be invoked whenever some significant event happens
  // as part of the protocol, after Start() is invoked.
  // The result of the protocol will be cached in |local_state|.
  // |power_initial| and |power_limit| are exponents of power-of-2 values which
  // will be the initial modulus and the maximum modulus used by this client.
  static std::unique_ptr<AutoEnrollmentClient> CreateForFRE(
      const ProgressCallback& progress_callback,
      DeviceManagementService* device_management_service,
      PrefService* local_state,
      scoped_refptr<net::URLRequestContextGetter> system_request_context,
      const std::string& server_backed_state_key,
      int power_initial,
      int power_limit);

  // |progress_callback| will be invoked whenever some significant event happens
  // as part of the protocol, after Start() is invoked.
  // The result of the protocol will be cached in |local_state|.
  // |power_initial| and |power_limit| are exponents of power-of-2 values which
  // will be the initial modulus and the maximum modulus used by this client.
  static std::unique_ptr<AutoEnrollmentClient> CreateForInitialEnrollment(
      const ProgressCallback& progress_callback,
      DeviceManagementService* device_management_service,
      PrefService* local_state,
      scoped_refptr<net::URLRequestContextGetter> system_request_context,
      const std::string& device_serial_number,
      const std::string& device_brand_code,
      int power_initial,
      int power_limit);

  ~AutoEnrollmentClient() override;

  // Registers preferences in local state.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  // Starts the auto-enrollment check protocol with the device management
  // service. Subsequent calls drop any previous requests. Notice that this
  // call can invoke the |progress_callback_| if errors occur.
  void Start();

  // Triggers a retry of the currently pending step. This is intended to be
  // called by consumers when they become aware of environment changes (such as
  // captive portal setup being complete).
  void Retry();

  // Cancels any pending requests. |progress_callback_| will not be invoked.
  // |this| will delete itself.
  void CancelAndDeleteSoon();

  // Returns the device_id randomly generated for the auto-enrollment requests.
  // It can be reused for subsequent requests to the device management service.
  std::string device_id() const { return device_id_; }

  // Current state.
  AutoEnrollmentState state() const { return state_; }

  // Implementation of net::NetworkChangeNotifier::NetworkChangeObserver:
  void OnNetworkChanged(
      net::NetworkChangeNotifier::ConnectionType type) override;

 private:
  typedef bool (AutoEnrollmentClient::*RequestCompletionHandler)(
      DeviceManagementStatus,
      int,
      const enterprise_management::DeviceManagementResponse&);

  AutoEnrollmentClient(
      const ProgressCallback& progress_callback,
      DeviceManagementService* device_management_service,
      PrefService* local_state,
      scoped_refptr<net::URLRequestContextGetter> system_request_context,
      std::unique_ptr<DeviceIdentifierProvider> device_identifier_provider,
      std::unique_ptr<StateDownloadMessageProcessor>
          state_download_message_processor,
      int power_initial,
      int power_limit,
      std::string uma_suffix);

  // Tries to load the result of a previous execution of the protocol from
  // local state. Returns true if that decision has been made and is valid.
  bool GetCachedDecision();

  // Kicks protocol processing, restarting the current step if applicable.
  // Returns true if progress has been made, false if the protocol is done.
  bool RetryStep();

  // Cleans up and invokes |progress_callback_|.
  void ReportProgress(AutoEnrollmentState state);

  // Calls RetryStep() to make progress or determine that all is done. In the
  // latter case, calls ReportProgress().
  void NextStep();

  // Sends an auto-enrollment check request to the device management service.
  void SendBucketDownloadRequest();

  // Sends a device state download request to the device management service.
  void SendDeviceStateRequest();

  // Runs the response handler for device management requests and calls
  // NextStep().
  void HandleRequestCompletion(
      RequestCompletionHandler handler,
      DeviceManagementStatus status,
      int net_error,
      const enterprise_management::DeviceManagementResponse& response);

  // Parses the server response to a bucket download request.
  bool OnBucketDownloadRequestCompletion(
      DeviceManagementStatus status,
      int net_error,
      const enterprise_management::DeviceManagementResponse& response);

  // Parses the server response to a device state request.
  bool OnDeviceStateRequestCompletion(
      DeviceManagementStatus status,
      int net_error,
      const enterprise_management::DeviceManagementResponse& response);

  // Returns true if the identifier hash provided by
  // |device_identifier_provider_| is contained in |hashes|.
  bool IsIdHashInProtobuf(
      const google::protobuf::RepeatedPtrField<std::string>& hashes);

  // Updates UMA histograms for bucket download timings.
  void UpdateBucketDownloadTimingHistograms();

  // Callback to invoke when the protocol generates a relevant event. This can
  // be either successful completion or an error that requires external action.
  ProgressCallback progress_callback_;

  // Current state.
  AutoEnrollmentState state_;

  // Whether the hash bucket check succeeded, indicating that the server knows
  // this device and might have keep state for it.
  bool has_server_state_;

  // Whether the download of server-kept device state completed successfully.
  bool device_state_available_;

  // Randomly generated device id for the auto-enrollment requests.
  std::string device_id_;

  // Power-of-2 modulus to try next.
  int current_power_;

  // Power of the maximum power-of-2 modulus that this client will accept from
  // a retry response from the server.
  int power_limit_;

  // Number of requests for a different modulus received from the server.
  // Used to determine if the server keeps asking for different moduli.
  int modulus_updates_received_;

  // Used to communicate with the device management service.
  DeviceManagementService* device_management_service_;
  std::unique_ptr<DeviceManagementRequestJob> request_job_;

  // PrefService where the protocol's results are cached.
  PrefService* local_state_;

  // The request context to use to perform the auto enrollment request.
  scoped_refptr<net::URLRequestContextGetter> request_context_;

  // Specifies the identifier set and the hash of the device's current
  // identifier.
  std::unique_ptr<DeviceIdentifierProvider> device_identifier_provider_;

  // Fills and parses state retrieval request / response.
  std::unique_ptr<StateDownloadMessageProcessor>
      state_download_message_processor_;

  // Times used to determine the duration of the protocol, and the extra time
  // needed to complete after the signin was complete.
  // If |time_start_| is not null, the protocol is still running.
  // If |time_extra_start_| is not null, the protocol is still running but our
  // owner has relinquished ownership.
  base::Time time_start_;
  base::Time time_extra_start_;

  // The time when the bucket download part of the protocol started.
  base::Time time_start_bucket_download_;

  // The UMA histogram suffix. Will be ".ForcedReenrollment" for an
  // |AutoEnrollmentClient| used for FRE and ".InitialEnrollment" for an
  // |AutoEnrollmentclient| used for initial enrollment.
  const std::string uma_suffix_;

  DISALLOW_COPY_AND_ASSIGN(AutoEnrollmentClient);
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_AUTO_ENROLLMENT_CLIENT_H_
