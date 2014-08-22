// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CLOUD_PRINT_GCP20_PROTOTYPE_PRIVET_HTTP_SERVER_H_
#define CLOUD_PRINT_GCP20_PROTOTYPE_PRIVET_HTTP_SERVER_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/values.h"
#include "cloud_print/gcp20/prototype/local_print_job.h"
#include "net/server/http_server.h"
#include "net/server/http_server_request_info.h"

class GURL;

// HTTP server for receiving .
class PrivetHttpServer: public net::HttpServer::Delegate {
 public:
  // TODO(maksymb): Move this enum to some namespace instead of this class.
  enum RegistrationErrorStatus {
    REG_ERROR_OK,

    REG_ERROR_INVALID_PARAMS,
    REG_ERROR_DEVICE_BUSY,
    REG_ERROR_PENDING_USER_ACTION,
    REG_ERROR_USER_CANCEL,
    REG_ERROR_CONFIRMATION_TIMEOUT,
    REG_ERROR_INVALID_ACTION,
    REG_ERROR_OFFLINE,
    REG_ERROR_SERVER_ERROR
  };

  // TODO(maksymb): Move this struct to some namespace instead of this class.
  struct DeviceInfo {
    DeviceInfo();
    ~DeviceInfo();

    std::string version;
    std::string name;
    std::string description;
    std::string url;
    std::string id;
    std::string device_state;
    std::string connection_state;
    std::string manufacturer;
    std::string model;
    std::string serial_number;
    std::string firmware;
    int uptime;
    std::string x_privet_token;

    std::vector<std::string> api;
    std::vector<std::string> type;
  };

  class Delegate {
   public:
    virtual ~Delegate() {}

    // Invoked when registration is starting.
    virtual RegistrationErrorStatus RegistrationStart(
        const std::string& user) = 0;

    // Invoked when claimtoken is needed.
    virtual RegistrationErrorStatus RegistrationGetClaimToken(
        const std::string& user,
        std::string* token,
        std::string* claim_url) = 0;

    // Invoked when registration is going to be completed.
    virtual RegistrationErrorStatus RegistrationComplete(
        const std::string& user,
        std::string* device_id) = 0;

    // Invoked when client asked for cancelling the registration.
    virtual RegistrationErrorStatus RegistrationCancel(
        const std::string& user) = 0;

    // Invoked for receiving server error details.
    virtual void GetRegistrationServerError(std::string* description) = 0;

    // Invoked when /privet/info is called.
    virtual void CreateInfo(DeviceInfo* info) = 0;

    // Invoked for checking wether /privet/register should be exposed.
    virtual bool IsRegistered() const = 0;

    // Invoked for checking wether /privet/printer/* should be exposed.
    virtual bool IsLocalPrintingAllowed() const = 0;

    // Invoked when XPrivetToken has to be checked.
    virtual bool CheckXPrivetTokenHeader(const std::string& token) const = 0;

    // Invoked for getting capabilities.
    virtual const base::DictionaryValue& GetCapabilities() = 0;

    // Invoked for creating a job.
    virtual LocalPrintJob::CreateResult CreateJob(
        const std::string& ticket,
        std::string* job_id,
        int* expires_in,
        // TODO(maksymb): Use base::TimeDelta for timeouts
        int* error_timeout,
        std::string* error_description) = 0;

    // Invoked for simple local printing.
    virtual LocalPrintJob::SaveResult SubmitDoc(
        const LocalPrintJob& job,
        std::string* job_id,
        int* expires_in,
        std::string* error_description,
        int* timeout) = 0;

    // Invoked for advanced local printing.
    virtual LocalPrintJob::SaveResult SubmitDocWithId(
        const LocalPrintJob& job,
        const std::string& job_id,
        int* expires_in,
        std::string* error_description,
        int* timeout) = 0;

    // Invoked for getting job status.
    virtual bool GetJobState(const std::string& job_id,
                             LocalPrintJob::Info* info) = 0;
  };

  // Constructor doesn't start server.
  explicit PrivetHttpServer(Delegate* delegate);

  // Destroys the object.
  virtual ~PrivetHttpServer();

  // Starts HTTP server: start listening port |port| for HTTP requests.
  bool Start(uint16 port);

  // Stops HTTP server.
  void Shutdown();

 private:
  // net::HttpServer::Delegate methods:
  virtual void OnHttpRequest(
      int connection_id,
      const net::HttpServerRequestInfo& info) OVERRIDE;
  virtual void OnWebSocketRequest(
      int connection_id,
      const net::HttpServerRequestInfo& info) OVERRIDE;
  virtual void OnWebSocketMessage(int connection_id,
                                  const std::string& data) OVERRIDE;
  virtual void OnClose(int connection_id) OVERRIDE;

  // Sends error as response. Invoked when request method is invalid.
  void ReportInvalidMethod(int connection_id);

  // Returns |true| if |request| should be done with correct |method|.
  // Otherwise sends |Invalid method| error.
  // Also checks support of |request| by this server.
  bool ValidateRequestMethod(int connection_id,
                             const std::string& request,
                             const std::string& method);

  // Processes http request after all preparations (XPrivetHeader check,
  // data handling etc.)
  net::HttpStatusCode ProcessHttpRequest(
      const GURL& url,
      const net::HttpServerRequestInfo& info,
      std::string* response);

  // Pivet API methods. Return reference to NULL if output should be empty.
  scoped_ptr<base::DictionaryValue> ProcessInfo(
      net::HttpStatusCode* status_code) const;

  scoped_ptr<base::DictionaryValue> ProcessCapabilities(
      net::HttpStatusCode* status_code) const;

  scoped_ptr<base::DictionaryValue> ProcessCreateJob(
      const GURL& url,
      const std::string& body,
      net::HttpStatusCode* status_code) const;

  scoped_ptr<base::DictionaryValue> ProcessSubmitDoc(
      const GURL& url,
      const net::HttpServerRequestInfo& info,
      net::HttpStatusCode* status_code) const;

  scoped_ptr<base::DictionaryValue> ProcessJobState(
      const GURL& url,
      net::HttpStatusCode* status_code) const;

  scoped_ptr<base::DictionaryValue> ProcessRegister(
      const GURL& url,
      net::HttpStatusCode* status_code) const;

  // Processes current status and depending on it replaces (or not)
  // |current_response| with error or empty response.
  void ProcessRegistrationStatus(
      RegistrationErrorStatus status,
      scoped_ptr<base::DictionaryValue>* current_response) const;

  // Port for listening.

  uint16 port_;

  // Contains encapsulated object for listening for requests.
  scoped_refptr<net::HttpServer> server_;

  Delegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(PrivetHttpServer);
};

#endif  // CLOUD_PRINT_GCP20_PROTOTYPE_PRIVET_HTTP_SERVER_H_

