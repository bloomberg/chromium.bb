// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICE_CLOUD_PRINT_JOB_STATUS_UPDATER_H_
#define CHROME_SERVICE_CLOUD_PRINT_JOB_STATUS_UPDATER_H_
#pragma once

#include <string>

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/threading/thread.h"
#include "chrome/service/cloud_print/cloud_print_url_fetcher.h"
#include "chrome/service/cloud_print/print_system.h"
#include "googleurl/src/gurl.h"
#include "net/url_request/url_request_status.h"

// Periodically monitors the status of a local print job and updates the
// cloud print server accordingly. When the job has been completed this
// object releases the reference to itself which should cause it to
// self-destruct.
class JobStatusUpdater : public base::RefCountedThreadSafe<JobStatusUpdater>,
                         public CloudPrintURLFetcherDelegate {
 public:
  class Delegate {
   public:
    virtual bool OnJobCompleted(JobStatusUpdater* updater) = 0;
    virtual void OnAuthError() = 0;

   protected:
    virtual ~Delegate() {}
  };

  JobStatusUpdater(const std::string& printer_name,
                   const std::string& job_id,
                   cloud_print::PlatformJobId& local_job_id,
                   const GURL& cloud_print_server_url,
                   cloud_print::PrintSystem* print_system,
                   Delegate* delegate);

  // Checks the status of the local print job and sends an update.
  void UpdateStatus();
  void Stop();

  // CloudPrintURLFetcher::Delegate implementation.
  virtual CloudPrintURLFetcher::ResponseAction HandleJSONData(
      const net::URLFetcher* source,
      const GURL& url,
      base::DictionaryValue* json_data,
      bool succeeded) OVERRIDE;
  virtual CloudPrintURLFetcher::ResponseAction OnRequestAuthError() OVERRIDE;
  virtual std::string GetAuthHeader() OVERRIDE;

 private:
  friend class base::RefCountedThreadSafe<JobStatusUpdater>;
  virtual ~JobStatusUpdater();

  std::string printer_name_;
  std::string job_id_;
  cloud_print::PlatformJobId local_job_id_;
  cloud_print::PrintJobDetails last_job_details_;
  scoped_refptr<CloudPrintURLFetcher> request_;
  GURL cloud_print_server_url_;
  scoped_refptr<cloud_print::PrintSystem> print_system_;
  Delegate* delegate_;
  // A flag that is set to true in Stop() and will ensure the next scheduled
  // task will do nothing.
  bool stopped_;
  DISALLOW_COPY_AND_ASSIGN(JobStatusUpdater);
};

// This typedef is to workaround the issue with certain versions of
// Visual Studio where it gets confused between multiple Delegate
// classes and gives a C2500 error. (I saw this error on the try bots -
// the workaround was not needed for my machine).
typedef JobStatusUpdater::Delegate JobStatusUpdaterDelegate;

#endif  // CHROME_SERVICE_CLOUD_PRINT_JOB_STATUS_UPDATER_H_
