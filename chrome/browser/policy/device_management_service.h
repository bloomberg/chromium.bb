// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_DEVICE_MANAGEMENT_SERVICE_H_
#define CHROME_BROWSER_POLICY_DEVICE_MANAGEMENT_SERVICE_H_
#pragma once

#include <deque>
#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/task.h"
#include "content/common/net/url_fetcher.h"
#include "googleurl/src/gurl.h"

namespace net {
class URLRequestContextGetter;
}

namespace policy {

class DeviceManagementBackend;

// The device management service is responsible for everything related to
// communication with the device management server. It creates the backends
// objects that the device management policy provider and friends use to issue
// requests.
class DeviceManagementService : public URLFetcher::Delegate {
 public:
  // Describes a device management job handled by the service.
  class DeviceManagementJob {
   public:
    virtual ~DeviceManagementJob() {}

    // Handles the URL request response.
    virtual void HandleResponse(const net::URLRequestStatus& status,
                                int response_code,
                                const net::ResponseCookies& cookies,
                                const std::string& data) = 0;

    // Gets the URL to contact.
    virtual GURL GetURL(const std::string& server_url) = 0;

    // Configures the fetcher, setting up payload and headers.
    virtual void ConfigureRequest(URLFetcher* fetcher) = 0;
  };

  explicit DeviceManagementService(const std::string& server_url);
  virtual ~DeviceManagementService();

  // Constructs a device management backend for use by client code. Ownership of
  // the returned backend object is transferred to the caller.
  // Marked virtual for the benefit of tests.
  virtual DeviceManagementBackend* CreateBackend();

  // Schedules a task to run |Initialize| after |delay_milliseconds| had passed.
  void ScheduleInitialization(int64 delay_milliseconds);

  // Makes the service stop all requests and drop the reference to the request
  // context.
  void Shutdown();

  // Adds a job. Caller must make sure the job pointer stays valid until the job
  // completes or gets canceled via RemoveJob().
  void AddJob(DeviceManagementJob* job);

  // Removes a job. The job will be removed and won't receive a completion
  // callback.
  void RemoveJob(DeviceManagementJob* job);

 protected:
  // Starts the given job.
  virtual void StartJob(DeviceManagementJob* job, bool bypass_proxy);

 private:
  typedef std::map<const URLFetcher*, DeviceManagementJob*> JobFetcherMap;
  typedef std::deque<DeviceManagementJob*> JobQueue;

  // URLFetcher::Delegate override.
  virtual void OnURLFetchComplete(const URLFetcher* source,
                                  const GURL& url,
                                  const net::URLRequestStatus& status,
                                  int response_code,
                                  const net::ResponseCookies& cookies,
                                  const std::string& data) OVERRIDE;

  // Does the actual initialization using the request context specified for
  // |PrepareInitialization|. This will also fire any pending network requests.
  void Initialize();

  // Server at which to contact the service.
  const std::string server_url_;

  // The request context we use.
  scoped_refptr<net::URLRequestContextGetter> request_context_getter_;

  // The jobs we currently have in flight.
  JobFetcherMap pending_jobs_;

  // Jobs that are registered, but not started yet.
  JobQueue queued_jobs_;

  // If this service is initialized, incoming requests get fired instantly.
  // If it is not initialized, incoming requests are queued.
  bool initialized_;

  // Creates tasks used for running |Initialize| delayed on the UI thread.
  ScopedRunnableMethodFactory<DeviceManagementService> method_factory_;

  DISALLOW_COPY_AND_ASSIGN(DeviceManagementService);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_DEVICE_MANAGEMENT_SERVICE_H_
