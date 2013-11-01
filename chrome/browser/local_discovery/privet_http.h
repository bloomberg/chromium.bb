// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LOCAL_DISCOVERY_PRIVET_HTTP_H_
#define CHROME_BROWSER_LOCAL_DISCOVERY_PRIVET_HTTP_H_

#include <string>

#include "base/callback.h"
#include "chrome/browser/local_discovery/privet_url_fetcher.h"
#include "net/base/host_port_pair.h"

namespace local_discovery {

class PrivetHTTPClient;

// Represents a request to /privet/info. Will store a cached response and token
// in the PrivetHTTPClient that created.
class PrivetInfoOperation {
 public:
  class Delegate {
   public:
    virtual ~Delegate() {}

    // In case of non-HTTP errors, |http_code| will be -1.

    // TODO(noamsml): Remove http_code from this delegate; it's unnecessary in
    // practice
    virtual void OnPrivetInfoDone(
        PrivetInfoOperation* operation,
        int http_code,
        const base::DictionaryValue* json_value) = 0;
  };

  virtual ~PrivetInfoOperation() {}

  virtual void Start() = 0;

  virtual PrivetHTTPClient* GetHTTPClient() = 0;
};

// Represents a full registration flow (/privet/register), normally consisting
// of calling the start action, the getClaimToken action, and calling the
// complete action. Some intervention from the caller is required to display the
// claim URL to the user (noted in OnPrivetRegisterClaimURL).
class PrivetRegisterOperation {
 public:
  enum FailureReason {
    FAILURE_NETWORK,
    FAILURE_HTTP_ERROR,
    FAILURE_JSON_ERROR,
    FAILURE_MALFORMED_RESPONSE,
    FAILURE_TOKEN,
    FAILURE_RETRY
  };

  class Delegate {
   public:
    ~Delegate() {}

    // Called when a user needs to claim the printer by visiting the given URL.
    virtual void OnPrivetRegisterClaimToken(
        PrivetRegisterOperation* operation,
        const std::string& token,
        const GURL& url) = 0;

    // TODO(noamsml): Remove all unnecessary parameters.
    // Called in case of an error while registering.  |action| is the
    // registration action taken during the error. |reason| is the reason for
    // the failure. |printer_http_code| is the http code returned from the
    // printer. If it is -1, an internal error occurred while trying to complete
    // the request. |json| may be null if printer_http_code signifies an error.
    virtual void OnPrivetRegisterError(PrivetRegisterOperation* operation,
                                       const std::string& action,
                                       FailureReason reason,
                                       int printer_http_code,
                                       const DictionaryValue* json) = 0;

    // Called when the registration is done.
    virtual void OnPrivetRegisterDone(PrivetRegisterOperation* operation,
                                      const std::string& device_id) = 0;
  };

  virtual ~PrivetRegisterOperation() {}

  virtual void Start() = 0;
  // Owner SHOULD call explicitly before destroying operation.
  virtual void Cancel() = 0;
  virtual void CompleteRegistration() = 0;

  virtual PrivetHTTPClient* GetHTTPClient() = 0;
};

class PrivetCapabilitiesOperation {
 public:
  class Delegate {
   public:
    virtual ~Delegate() {}

    // |capabilities| will be NULL in case of an error.
    virtual void OnPrivetCapabilities(
        PrivetCapabilitiesOperation* capabilities_operation,
        int http_error,
        const base::DictionaryValue* capabilities) = 0;
  };

  virtual ~PrivetCapabilitiesOperation() {}
  virtual void Start() = 0;

  virtual PrivetHTTPClient* GetHTTPClient() = 0;
};

class PrivetLocalPrintOperation {
 public:
  class Delegate {
   public:
    virtual ~Delegate() {}
    virtual void OnPrivetPrintingRequestPDF(
        const PrivetLocalPrintOperation* print_operation) = 0;
    virtual void OnPrivetPrintingRequestPWGRaster(
        const PrivetLocalPrintOperation* print_operation) = 0;
    virtual void OnPrivetPrintingDone(
        const PrivetLocalPrintOperation* print_operation) = 0;
    virtual void OnPrivetPrintingError(
        const PrivetLocalPrintOperation* print_operation, int http_code) = 0;
  };

  virtual ~PrivetLocalPrintOperation() {}

  virtual void Start() = 0;

  // Should be called ONLY after |OnPrivetPrintingRequestPDF| or
  // |OnPrivetPrintingRequestPWGRaster| are called on the delegate.  Data should
  // be in PDF or PWG format depending on what is requested by the local print
  // operation.
  virtual void SendData(const std::string& data) = 0;

  // Optional attributes for /submitdoc. Call before calling |Start()|
  // |ticket| should be in CJT format.
  virtual void SetTicket(const std::string& ticket) = 0;
  // Username and jobname are for display only.
  virtual void SetUsername(const std::string& username) = 0;
  virtual void SetJobname(const std::string& jobname) = 0;
  // If |offline| is true, we will indicate to the printer not to post the job
  // to Google Cloud Print.
  virtual void SetOffline(bool offline) = 0;

  virtual PrivetHTTPClient* GetHTTPClient() = 0;
};

// Privet HTTP client. Must not outlive the operations it creates.
class PrivetHTTPClient {
 public:
  virtual ~PrivetHTTPClient() {}
  virtual const base::DictionaryValue* GetCachedInfo() const = 0;

  virtual scoped_ptr<PrivetRegisterOperation> CreateRegisterOperation(
      const std::string& user,
      PrivetRegisterOperation::Delegate* delegate) = 0;
  virtual scoped_ptr<PrivetInfoOperation> CreateInfoOperation(
      PrivetInfoOperation::Delegate* delegate) = 0;
  virtual scoped_ptr<PrivetCapabilitiesOperation> CreateCapabilitiesOperation(
      PrivetCapabilitiesOperation::Delegate* delegate) = 0;
  virtual scoped_ptr<PrivetLocalPrintOperation> CreateLocalPrintOperation(
      PrivetLocalPrintOperation::Delegate* delegate) = 0;

  // A name for the HTTP client, e.g. the device name for the privet device.
  virtual const std::string& GetName() = 0;
};

}  // namespace local_discovery
#endif  // CHROME_BROWSER_LOCAL_DISCOVERY_PRIVET_HTTP_H_
