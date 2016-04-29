// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_CLOUD_DEVICE_MANAGEMENT_SERVICE_H_
#define COMPONENTS_POLICY_CORE_COMMON_CLOUD_DEVICE_MANAGEMENT_SERVICE_H_

#include <stdint.h>

#include <deque>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_split.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/policy_export.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "policy/proto/device_management_backend.pb.h"


namespace net {
class URLRequestContextGetter;
}

namespace policy {

class DeviceManagementRequestJobImpl;
class DeviceManagementService;

// DeviceManagementRequestJob describes a request to send to the device
// management service. Jobs are created by DeviceManagementService. They can be
// canceled by deleting the object.
class POLICY_EXPORT DeviceManagementRequestJob {
 public:
  // Describes the job type.
  enum JobType {
    TYPE_AUTO_ENROLLMENT,
    TYPE_REGISTRATION,
    TYPE_API_AUTH_CODE_FETCH,
    TYPE_POLICY_FETCH,
    TYPE_UNREGISTRATION,
    TYPE_UPLOAD_CERTIFICATE,
    TYPE_DEVICE_STATE_RETRIEVAL,
    TYPE_UPLOAD_STATUS,
    TYPE_REMOTE_COMMANDS,
    TYPE_ATTRIBUTE_UPDATE_PERMISSION,
    TYPE_ATTRIBUTE_UPDATE,
    TYPE_GCM_ID_UPDATE,
    TYPE_ANDROID_MANAGEMENT_CHECK,
  };

  typedef base::Callback<
      void(DeviceManagementStatus, int,
           const enterprise_management::DeviceManagementResponse&)> Callback;

  typedef base::Callback<void(DeviceManagementRequestJob*)> RetryCallback;

  virtual ~DeviceManagementRequestJob();

  // Functions for configuring the job. These should only be called before
  // Start()ing the job, but never afterwards.
  void SetGaiaToken(const std::string& gaia_token);
  void SetOAuthToken(const std::string& oauth_token);
  void SetDMToken(const std::string& dm_token);
  void SetClientID(const std::string& client_id);
  enterprise_management::DeviceManagementRequest* GetRequest();

  // A job may automatically retry if it fails due to a temporary condition, or
  // due to proxy misconfigurations. If a |retry_callback| is set then it will
  // be invoked with the DeviceManagementRequestJob as an argument when that
  // happens, so that the job's owner can customize the retry request before
  // it's sent.
  void SetRetryCallback(const RetryCallback& retry_callback);

  // Starts the job. |callback| will be invoked on completion.
  void Start(const Callback& callback);

 protected:
  typedef base::StringPairs ParameterMap;

  DeviceManagementRequestJob(JobType type,
                             const std::string& agent_parameter,
                             const std::string& platform_parameter);

  // Appends a parameter to |query_params|.
  void AddParameter(const std::string& name, const std::string& value);

  // Fires the job, to be filled in by implementations.
  virtual void Run() = 0;

  ParameterMap query_params_;
  std::string gaia_token_;
  std::string dm_token_;
  enterprise_management::DeviceManagementRequest request_;
  RetryCallback retry_callback_;

  Callback callback_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DeviceManagementRequestJob);
};

// The device management service is responsible for everything related to
// communication with the device management server. It creates the backends
// objects that the device management policy provider and friends use to issue
// requests.
class POLICY_EXPORT DeviceManagementService : public net::URLFetcherDelegate {
 public:
  // Obtains the parameters used to contact the server.
  // This allows creating the DeviceManagementService early and getting these
  // parameters later. Passing the parameters directly in the ctor isn't
  // possible because some aren't ready during startup. http://crbug.com/302798
  class POLICY_EXPORT Configuration {
   public:
    virtual ~Configuration() {}

    // Server at which to contact the service.
    virtual std::string GetServerUrl() = 0;

    // Agent reported in the "agent" query parameter.
    virtual std::string GetAgentParameter() = 0;

    // The platform reported in the "platform" query parameter.
    virtual std::string GetPlatformParameter() = 0;
  };

  explicit DeviceManagementService(
      std::unique_ptr<Configuration> configuration);
  ~DeviceManagementService() override;

  // The ID of URLFetchers created by the DeviceManagementService. This can be
  // used by tests that use a TestURLFetcherFactory to get the pending fetchers
  // created by the DeviceManagementService.
  static const int kURLFetcherID;

  // Creates a new device management request job. Ownership is transferred to
  // the caller.
  virtual DeviceManagementRequestJob* CreateJob(
      DeviceManagementRequestJob::JobType type,
      const scoped_refptr<net::URLRequestContextGetter>& request_context);

  // Schedules a task to run |Initialize| after |delay_milliseconds| had passed.
  void ScheduleInitialization(int64_t delay_milliseconds);

  // Makes the service stop all requests.
  void Shutdown();

  // Gets the URL that the DMServer requests are sent to.
  std::string GetServerUrl();

 private:
  typedef std::map<const net::URLFetcher*,
                   DeviceManagementRequestJobImpl*> JobFetcherMap;
  typedef std::deque<DeviceManagementRequestJobImpl*> JobQueue;

  friend class DeviceManagementRequestJobImpl;

  // net::URLFetcherDelegate override.
  void OnURLFetchComplete(const net::URLFetcher* source) override;

  // Starts processing any queued jobs.
  void Initialize();

  // Starts a job.
  void StartJob(DeviceManagementRequestJobImpl* job);

  // Adds a job. Caller must make sure the job pointer stays valid until the job
  // completes or gets canceled via RemoveJob().
  void AddJob(DeviceManagementRequestJobImpl* job);

  // Removes a job. The job will be removed and won't receive a completion
  // callback.
  void RemoveJob(DeviceManagementRequestJobImpl* job);

  // A Configuration implementation that is used to obtain various parameters
  // used to talk to the device management server.
  std::unique_ptr<Configuration> configuration_;

  // The jobs we currently have in flight.
  JobFetcherMap pending_jobs_;

  // Jobs that are registered, but not started yet.
  JobQueue queued_jobs_;

  // If this service is initialized, incoming requests get fired instantly.
  // If it is not initialized, incoming requests are queued.
  bool initialized_;

  // Used to create tasks to run |Initialize| delayed on the UI thread.
  base::WeakPtrFactory<DeviceManagementService> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(DeviceManagementService);
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_CLOUD_DEVICE_MANAGEMENT_SERVICE_H_
