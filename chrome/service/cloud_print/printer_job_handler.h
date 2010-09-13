// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICE_CLOUD_PRINT_PRINTER_JOB_HANDLER_H_
#define CHROME_SERVICE_CLOUD_PRINT_PRINTER_JOB_HANDLER_H_
#pragma once

#include <list>
#include <string>

#include "base/file_path.h"
#include "base/ref_counted.h"
#include "base/message_loop_proxy.h"
#include "base/thread.h"
#include "chrome/service/cloud_print/job_status_updater.h"
#include "chrome/service/cloud_print/print_system.h"
#include "chrome/common/net/url_fetcher.h"
#include "googleurl/src/gurl.h"
#include "net/url_request/url_request_status.h"

// A class that handles cloud print jobs for a particular printer. This class
// imlements a state machine that transitions from Start to various states. The
// various states are shown in the below diagram.
// the status on the server.

//                            Start --> No pending tasks --> Done
//                              |
//                              |
//                              | Have Pending tasks
//                              |
//                              |
//       <----Delete Pending -- | ---Update Pending----->
//       |                      |                       |
//       |                      |                       |
//       |                      |                       |
// Delete Printer from server   |                 Update Printer info on server
//   Shutdown                   |                      Go to Stop
//                              |
//                              | Job Available
//                              |
//                              |
//                        Fetch Next Job Metadata
//                        Fetch Print Ticket
//                        Fetch Print Data
//                        Spool Print Job
//                        Create Job StatusUpdater for job
//                        Mark job as "in progress" on server
//     (On any unrecoverable error in any of the above steps go to Stop)
//                        Go to Stop
//                              |
//                              |
//                              |
//                              |
//                              |
//                              |
//                              |
//                             Stop
//               (If there are pending tasks go back to Start)

typedef URLFetcher::Delegate URLFetcherDelegate;

class PrinterJobHandler : public base::RefCountedThreadSafe<PrinterJobHandler>,
                          public URLFetcherDelegate,
                          public JobStatusUpdaterDelegate,
                          public cloud_print::PrinterWatcherDelegate,
                          public cloud_print::JobSpoolerDelegate {
  enum PrintJobError {
    SUCCESS,
    JOB_DOWNLOAD_FAILED,
    INVALID_JOB_DATA,
    PRINT_FAILED,
  };
  struct JobDetails {
    std::string job_id_;
    std::string job_title_;
    std::string print_ticket_;
    FilePath print_data_file_path_;
    std::string print_data_mime_type_;
    void Clear() {
      job_id_.clear();
      job_title_.clear();
      print_ticket_.clear();
      print_data_mime_type_.clear();
      print_data_file_path_ = FilePath();
    }
  };

 public:
  class Delegate {
   public:
     virtual void OnPrinterJobHandlerShutdown(
        PrinterJobHandler* job_handler, const std::string& printer_id) = 0;
     virtual void OnAuthError() = 0;

   protected:
     virtual ~Delegate() {}
  };

  struct PrinterInfoFromCloud {
    std::string printer_id;
    std::string caps_hash;
    std::string tags_hash;
  };

  // Begin public interface
  PrinterJobHandler(const cloud_print::PrinterBasicInfo& printer_info,
                    const PrinterInfoFromCloud& printer_info_from_server,
                    const std::string& auth_token,
                    const GURL& cloud_print_server_url,
                    cloud_print::PrintSystem* print_system,
                    Delegate* delegate);
  ~PrinterJobHandler();
  bool Initialize();
  // Notifies the JobHandler that a job is available
  void NotifyJobAvailable();
  // Shutdown everything (the process is exiting).
  void Shutdown();
  // End public interface

  // Begin Delegate implementations

  // URLFetcher::Delegate implementation.
  virtual void OnURLFetchComplete(const URLFetcher* source, const GURL& url,
                                  const URLRequestStatus& status,
                                  int response_code,
                                  const ResponseCookies& cookies,
                                  const std::string& data);
  // JobStatusUpdater::Delegate implementation
  virtual bool OnJobCompleted(JobStatusUpdater* updater);
  virtual void OnAuthError();

  // cloud_print::PrinterWatcherDelegate implementation
  virtual void OnPrinterDeleted();
  virtual void OnPrinterChanged();
  virtual void OnJobChanged();

  // cloud_print::JobSpoolerDelegate implementation.
  // Called on print_thread_.
  virtual void OnJobSpoolSucceeded(const cloud_print::PlatformJobId& job_id);
  virtual void OnJobSpoolFailed();

  // End Delegate implementations

 private:
  // Prototype for a response handler. The return value indicates whether the
  // request should be retried, false means "retry", true means "do not retry"
  typedef bool (PrinterJobHandler::*ResponseHandler)(
      const URLFetcher* source, const GURL& url,
      const URLRequestStatus& status, int response_code,
      const ResponseCookies& cookies, const std::string& data);
  // Prototype for a failure handler. This handler will be executed if all
  // attempts to fetch url failed.
  typedef void (PrinterJobHandler::*FailureHandler)();
  // Begin request handlers for each state in the state machine
  bool HandlePrinterUpdateResponse(const URLFetcher* source, const GURL& url,
                                   const URLRequestStatus& status,
                                   int response_code,
                                   const ResponseCookies& cookies,
                                   const std::string& data);
  bool HandlePrinterDeleteResponse(const URLFetcher* source, const GURL& url,
                                   const URLRequestStatus& status,
                                   int response_code,
                                   const ResponseCookies& cookies,
                                   const std::string& data);
  bool HandleJobMetadataResponse(const URLFetcher* source, const GURL& url,
                                 const URLRequestStatus& status,
                                 int response_code,
                                 const ResponseCookies& cookies,
                                 const std::string& data);
  bool HandlePrintTicketResponse(const URLFetcher* source,
                                 const GURL& url,
                                 const URLRequestStatus& status,
                                 int response_code,
                                 const ResponseCookies& cookies,
                                 const std::string& data);
  bool HandlePrintDataResponse(const URLFetcher* source,
                               const GURL& url,
                               const URLRequestStatus& status,
                               int response_code,
                               const ResponseCookies& cookies,
                               const std::string& data);
  bool HandleSuccessStatusUpdateResponse(const URLFetcher* source,
                                         const GURL& url,
                                         const URLRequestStatus& status,
                                         int response_code,
                                         const ResponseCookies& cookies,
                                         const std::string& data);
  bool HandleFailureStatusUpdateResponse(const URLFetcher* source,
                                         const GURL& url,
                                         const URLRequestStatus& status,
                                         int response_code,
                                         const ResponseCookies& cookies,
                                         const std::string& data);
  // End request handlers for each state in the state machine

  // Start the state machine. Based on the flags set this could mean updating
  // printer information, deleting the printer from the server or looking for
  // new print jobs
  void Start();

  // End the state machine. If there are pending tasks, we will post a Start
  // again.
  void Stop();

  void StartPrinting();
  void HandleServerError(const GURL& url);
  void Reset();
  void UpdateJobStatus(cloud_print::PrintJobStatus status, PrintJobError error);

  // This function should be used to go from one state to another. It will
  // retry to fetch url (up to a limit) if response handler will return false.
  // Calling this function will zero failure counter.
  void MakeServerRequest(const GURL& url,
                         ResponseHandler response_handler,
                         FailureHandler failure_handler);
  // This function should be used ONLY from MakeServerRequest and
  // HandleServerError. It is using stored handlers and failure counter
  // to decide which handler to call.
  void FetchURL(const GURL& url);

  void JobFailed(PrintJobError error);
  void JobSpooled(cloud_print::PlatformJobId local_job_id);
  // Returns false if printer info is up to date and no updating is needed.
  bool UpdatePrinterInfo();
  bool HavePendingTasks();
  void FailedFetchingJobData();

  // Called on print_thread_.
  void DoPrint(const JobDetails& job_details,
               const std::string& printer_name);

  scoped_ptr<URLFetcher> request_;
  scoped_refptr<cloud_print::PrintSystem> print_system_;
  cloud_print::PrinterBasicInfo printer_info_;
  PrinterInfoFromCloud printer_info_cloud_;
  std::string auth_token_;
  GURL cloud_print_server_url_;
  std::string print_data_url_;
  JobDetails job_details_;
  Delegate* delegate_;
  // Once the job has been spooled to the local spooler, this specifies the
  // job id of the job on the local spooler.
  cloud_print::PlatformJobId local_job_id_;
  ResponseHandler next_response_handler_;
  FailureHandler next_failure_handler_;
  // The number of consecutive times that connecting to the server failed.
  int server_error_count_;
  // The thread on which the actual print operation happens
  base::Thread print_thread_;
  // The Job spooler object. This is only non-NULL during a print operation.
  // It lives and dies on |print_thread_|
  scoped_refptr<cloud_print::PrintSystem::JobSpooler> job_spooler_;
  // The message loop proxy representing the thread on which this object
  // was created. Used by the print thread.
  scoped_refptr<base::MessageLoopProxy> job_handler_message_loop_proxy_;

  // There may be pending tasks in the message queue when Shutdown is called.
  // We set this flag so as to do nothing in those tasks.
  bool shutting_down_;

  // Flags that specify various pending server updates
  bool server_job_available_;
  bool printer_update_pending_;
  bool printer_delete_pending_;

  // Some task in the state machine is in progress.
  bool task_in_progress_;
  scoped_refptr<cloud_print::PrintSystem::PrinterWatcher> printer_watcher_;
  typedef std::list< scoped_refptr<JobStatusUpdater> > JobStatusUpdaterList;
  JobStatusUpdaterList job_status_updater_list_;

  DISALLOW_COPY_AND_ASSIGN(PrinterJobHandler);
};

// This typedef is to workaround the issue with certain versions of
// Visual Studio where it gets confused between multiple Delegate
// classes and gives a C2500 error. (I saw this error on the try bots -
// the workaround was not needed for my machine).
typedef PrinterJobHandler::Delegate PrinterJobHandlerDelegate;

#endif  // CHROME_SERVICE_CLOUD_PRINT_PRINTER_JOB_HANDLER_H_

