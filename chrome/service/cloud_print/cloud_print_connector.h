// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICE_CLOUD_PRINT_CLOUD_PRINT_CONNECTOR_H_
#define CHROME_SERVICE_CLOUD_PRINT_CLOUD_PRINT_CONNECTOR_H_
#pragma once

#include <list>
#include <map>
#include <string>

#include "base/threading/thread.h"
#include "base/values.h"
#include "chrome/service/cloud_print/print_system.h"
#include "chrome/service/cloud_print/printer_job_handler.h"

// CloudPrintConnector handles top printer management tasks.
//  - Matching local and cloud printers
//  - Registration of local printers
//  - Deleting cloud printers
// All tasks are posted to the commond queue (PendingTasks) and executed
// one-by-one in FIFO order.
// CloudPrintConnector will notify client over Client interface.
class CloudPrintConnector
    : public base::RefCountedThreadSafe<CloudPrintConnector>,
      public cloud_print::PrintServerWatcherDelegate,
      public PrinterJobHandlerDelegate,
      public CloudPrintURLFetcherDelegate {
 public:
  class Client {
   public:
    virtual void OnAuthFailed() = 0;
   protected:
     virtual ~Client() {}
  };

  CloudPrintConnector(Client* client,
                      const std::string& proxy_id,
                      const GURL& cloud_print_server_url,
                      const DictionaryValue* print_system_settings);

  bool Start();
  void Stop();
  bool IsRunning();

  // Return list of printer ids registered with CloudPrint.
  void GetPrinterIds(std::list<std::string>* printer_ids);

  // Register printer from the list.
  void RegisterPrinters(const printing::PrinterList& printers);

  // Check for jobs for specific printer. If printer id is empty
  // jobs will be checked for all available printers.
  void CheckForJobs(const std::string& reason, const std::string& printer_id);

  // cloud_print::PrintServerWatcherDelegate implementation
  virtual void OnPrinterAdded() OVERRIDE;
  // PrinterJobHandler::Delegate implementation
  virtual void OnPrinterDeleted(const std::string& printer_name) OVERRIDE;
  virtual void OnAuthError() OVERRIDE;

  // CloudPrintURLFetcher::Delegate implementation.
  virtual CloudPrintURLFetcher::ResponseAction HandleRawData(
      const net::URLFetcher* source,
      const GURL& url,
      const std::string& data) OVERRIDE;
  virtual CloudPrintURLFetcher::ResponseAction HandleJSONData(
      const net::URLFetcher* source,
      const GURL& url,
      base::DictionaryValue* json_data,
      bool succeeded) OVERRIDE;
  virtual CloudPrintURLFetcher::ResponseAction OnRequestAuthError() OVERRIDE;
  virtual std::string GetAuthHeader() OVERRIDE;

 private:
  friend class base::RefCountedThreadSafe<CloudPrintConnector>;

  // Prototype for a response handler.
  typedef CloudPrintURLFetcher::ResponseAction
      (CloudPrintConnector::*ResponseHandler)(
          const net::URLFetcher* source,
          const GURL& url,
          DictionaryValue* json_data,
          bool succeeded);

  enum PendingTaskType {
    PENDING_PRINTERS_NONE,
    PENDING_PRINTERS_AVAILABLE,
    PENDING_PRINTER_REGISTER,
    PENDING_PRINTER_DELETE
  };

  // TODO(jhawkins): This name conflicts with base::PendingTask.
  struct PendingTask {
    PendingTaskType type;
    // Optional members, depending on type.
    std::string printer_id;  // For pending delete.
    printing::PrinterBasicInfo printer_info;  // For pending registration.

    PendingTask() : type(PENDING_PRINTERS_NONE) {}
    ~PendingTask() {}
  };

  virtual ~CloudPrintConnector();

  // Begin response handlers
  CloudPrintURLFetcher::ResponseAction HandlePrinterListResponse(
      const net::URLFetcher* source,
      const GURL& url,
      DictionaryValue* json_data,
      bool succeeded);

  CloudPrintURLFetcher::ResponseAction HandlePrinterDeleteResponse(
      const net::URLFetcher* source,
      const GURL& url,
      DictionaryValue* json_data,
      bool succeeded);

  CloudPrintURLFetcher::ResponseAction HandleRegisterPrinterResponse(
      const net::URLFetcher* source,
      const GURL& url,
      DictionaryValue* json_data,
      bool succeeded);
  // End response handlers

  // Helper functions for network requests.
  void StartGetRequest(const GURL& url,
                       int max_retries,
                       ResponseHandler handler);
  void StartPostRequest(const GURL& url,
                        int max_retries,
                        const std::string& mime_type,
                        const std::string& post_data,
                        ResponseHandler handler);

  // Reports a diagnostic message to the server.
  void ReportUserMessage(const std::string& message_id,
                         const std::string& failure_message);

  bool RemovePrinterFromList(const std::string& printer_name,
                             printing::PrinterList* printer_list);

  void InitJobHandlerForPrinter(DictionaryValue* printer_data);

  void AddPendingAvailableTask();
  void AddPendingDeleteTask(const std::string& id);
  void AddPendingRegisterTask(const printing::PrinterBasicInfo& info);
  void AddPendingTask(const PendingTask& task);
  void ProcessPendingTask();
  void ContinuePendingTaskProcessing();
  void OnPrintersAvailable();
  void OnPrinterRegister(const printing::PrinterBasicInfo& info);
  void OnPrinterDelete(const std::string& name);

  void OnReceivePrinterCaps(
      bool succeeded,
      const std::string& printer_name,
      const printing::PrinterCapsAndDefaults& caps_and_defaults);

  bool IsSamePrinter(const std::string& name1, const std::string& name2) const;

  // CloudPrintConnector client.
  Client* client_;
  // Print system settings.
  scoped_ptr<DictionaryValue> print_system_settings_;
  // Pointer to current print system.
  scoped_refptr<cloud_print::PrintSystem> print_system_;
  // Watcher for print system updates.
  scoped_refptr<cloud_print::PrintSystem::PrintServerWatcher>
      print_server_watcher_;
  // Id of the Cloud Print proxy.
  std::string proxy_id_;
  // Cloud Print server url.
  GURL cloud_print_server_url_;
  // A map of printer id to job handler.
  typedef std::map<std::string, scoped_refptr<PrinterJobHandler> >
      JobHandlerMap;
  JobHandlerMap job_handler_map_;
  // Next response handler.
  ResponseHandler next_response_handler_;
  // The list of peding tasks to be done in the background.
  std::list<PendingTask> pending_tasks_;
  // The CloudPrintURLFetcher instance for the current request.
  scoped_refptr<CloudPrintURLFetcher> request_;
  // The CloudPrintURLFetcher instance for the user message request.
  scoped_refptr<CloudPrintURLFetcher> user_message_request_;

  DISALLOW_COPY_AND_ASSIGN(CloudPrintConnector);
};

#endif  // CHROME_SERVICE_CLOUD_PRINT_CLOUD_PRINT_CONNECTOR_H_

