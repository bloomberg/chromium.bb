// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICE_CLOUD_PRINT_PRINTER_JOB_HANDLER_H_
#define CHROME_SERVICE_CLOUD_PRINT_PRINTER_JOB_HANDLER_H_
#pragma once

#include <list>
#include <string>
#include <vector>

#include "base/file_path.h"
#include "base/ref_counted.h"
#include "base/message_loop_proxy.h"
#include "base/threading/thread.h"
#include "base/time.h"
#include "chrome/service/cloud_print/cloud_print_url_fetcher.h"
#include "chrome/service/cloud_print/job_status_updater.h"
#include "googleurl/src/gurl.h"
#include "net/url_request/url_request_status.h"
#include "printing/backend/print_backend.h"

class URLFetcher;
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

class PrinterJobHandler : public base::RefCountedThreadSafe<PrinterJobHandler>,
                          public CloudPrintURLFetcherDelegate,
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
    JobDetails();
    ~JobDetails();
    void Clear();

    std::string job_id_;
    std::string job_title_;
    std::string print_ticket_;
    FilePath print_data_file_path_;
    std::string print_data_mime_type_;
    std::vector<std::string> tags_;
  };

 public:
  class Delegate {
   public:
     virtual void OnPrinterJobHandlerShutdown(
        PrinterJobHandler* job_handler, const std::string& printer_id) = 0;
     virtual void OnAuthError() = 0;
     // Called when the PrinterJobHandler cannot find the printer locally. The
     // delegate returns |delete_from_server| to true if the printer should be
     // deleted from the server,false otherwise.
     virtual void OnPrinterNotFound(const std::string& printer_name,
                                    bool* delete_from_server) = 0;

   protected:
     virtual ~Delegate() {}
  };

  struct PrinterInfoFromCloud {
    std::string printer_id;
    std::string caps_hash;
    std::string tags_hash;
  };

  // Begin public interface
  PrinterJobHandler(const printing::PrinterBasicInfo& printer_info,
                    const PrinterInfoFromCloud& printer_info_from_server,
                    const std::string& auth_token,
                    const GURL& cloud_print_server_url,
                    cloud_print::PrintSystem* print_system,
                    Delegate* delegate);
  ~PrinterJobHandler();
  bool Initialize();
  // Requests a job check. |reason| is the reason for fetching the job. Used
  // for logging and diagnostc purposes.
  void CheckForJobs(const std::string& reason);
  // Shutdown everything (the process is exiting).
  void Shutdown();
  base::TimeTicks last_job_fetch_time() const { return last_job_fetch_time_; }
  // End public interface

  // Begin Delegate implementations

  // CloudPrintURLFetcher::Delegate implementation.
  virtual CloudPrintURLFetcher::ResponseAction HandleRawData(
      const URLFetcher* source,
      const GURL& url,
      const std::string& data);
  virtual CloudPrintURLFetcher::ResponseAction HandleJSONData(
      const URLFetcher* source,
      const GURL& url,
      DictionaryValue* json_data,
      bool succeeded);
  virtual void OnRequestGiveUp();
  virtual void OnRequestAuthError();

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
  // Prototype for a JSON data handler.
  typedef CloudPrintURLFetcher::ResponseAction
      (PrinterJobHandler::*JSONDataHandler)(const URLFetcher* source,
                                            const GURL& url,
                                            DictionaryValue* json_data,
                                            bool succeeded);
  // Prototype for a data handler.
  typedef CloudPrintURLFetcher::ResponseAction
      (PrinterJobHandler::*DataHandler)(const URLFetcher* source,
                                        const GURL& url,
                                        const std::string& data);
  // Begin request handlers for each state in the state machine
  CloudPrintURLFetcher::ResponseAction HandlePrinterUpdateResponse(
      const URLFetcher* source,
      const GURL& url,
      DictionaryValue* json_data,
      bool succeeded);

  CloudPrintURLFetcher::ResponseAction HandlePrinterDeleteResponse(
      const URLFetcher* source,
      const GURL& url,
      DictionaryValue* json_data,
      bool succeeded);

  CloudPrintURLFetcher::ResponseAction HandleJobMetadataResponse(
      const URLFetcher* source,
      const GURL& url,
      DictionaryValue* json_data,
      bool succeeded);

  CloudPrintURLFetcher::ResponseAction HandlePrintTicketResponse(
      const URLFetcher* source,
      const GURL& url,
      const std::string& data);

  CloudPrintURLFetcher::ResponseAction HandlePrintDataResponse(
      const URLFetcher* source,
      const GURL& url,
      const std::string& data);

  CloudPrintURLFetcher::ResponseAction HandleSuccessStatusUpdateResponse(
      const URLFetcher* source,
      const GURL& url,
      DictionaryValue* json_data,
      bool succeeded);

  CloudPrintURLFetcher::ResponseAction HandleFailureStatusUpdateResponse(
      const URLFetcher* source,
      const GURL& url,
      DictionaryValue* json_data,
      bool succeeded);
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

  // Sets the next response handler to the specifed JSON data handler.
  void SetNextJSONHandler(JSONDataHandler handler);
  // Sets the next response handler to the specifed data handler.
  void SetNextDataHandler(DataHandler handler);

  void JobFailed(PrintJobError error);
  void JobSpooled(cloud_print::PlatformJobId local_job_id);
  // Returns false if printer info is up to date and no updating is needed.
  bool UpdatePrinterInfo();
  bool HavePendingTasks();
  void FailedFetchingJobData();

  // Callback that asynchronously receives printer caps and defaults.
  void OnReceivePrinterCaps(
    bool succeeded,
    const std::string& printer_name,
    const printing::PrinterCapsAndDefaults& caps_and_defaults);

  // Called on print_thread_.
  void DoPrint(const JobDetails& job_details,
               const std::string& printer_name);

  scoped_refptr<CloudPrintURLFetcher> request_;
  scoped_refptr<cloud_print::PrintSystem> print_system_;
  printing::PrinterBasicInfo printer_info_;
  PrinterInfoFromCloud printer_info_cloud_;
  std::string auth_token_;
  GURL cloud_print_server_url_;
  std::string print_data_url_;
  JobDetails job_details_;
  Delegate* delegate_;
  // Once the job has been spooled to the local spooler, this specifies the
  // job id of the job on the local spooler.
  cloud_print::PlatformJobId local_job_id_;

  // The next response handler can either be a JSONDataHandler or a
  // DataHandler (depending on the current request being made).
  JSONDataHandler next_json_data_handler_;
  DataHandler next_data_handler_;
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

  // A string indicating the reason we are fetching jobs from the server
  // (used to specify the reason in the fetch URL).
  std::string job_fetch_reason_;
  // Flags that specify various pending server updates
  bool job_check_pending_;
  bool printer_update_pending_;
  bool printer_delete_pending_;

  // Some task in the state machine is in progress.
  bool task_in_progress_;
  scoped_refptr<cloud_print::PrintSystem::PrinterWatcher> printer_watcher_;
  typedef std::list< scoped_refptr<JobStatusUpdater> > JobStatusUpdaterList;
  JobStatusUpdaterList job_status_updater_list_;

  base::TimeTicks last_job_fetch_time_;

  DISALLOW_COPY_AND_ASSIGN(PrinterJobHandler);
};

// This typedef is to workaround the issue with certain versions of
// Visual Studio where it gets confused between multiple Delegate
// classes and gives a C2500 error. (I saw this error on the try bots -
// the workaround was not needed for my machine).
typedef PrinterJobHandler::Delegate PrinterJobHandlerDelegate;

#endif  // CHROME_SERVICE_CLOUD_PRINT_PRINTER_JOB_HANDLER_H_
