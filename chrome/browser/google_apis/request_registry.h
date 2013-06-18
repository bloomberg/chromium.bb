// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GOOGLE_APIS_REQUEST_REGISTRY_H_
#define CHROME_BROWSER_GOOGLE_APIS_REQUEST_REGISTRY_H_

#include "base/basictypes.h"
#include "base/files/file_path.h"
#include "base/id_map.h"
#include "chrome/browser/google_apis/gdata_errorcode.h"

namespace google_apis {

// Unique ID to identify each request.
typedef int32 RequestID;

// Enumeration type for indicating the state of the transfer.
enum RequestTransferState {
  REQUEST_NOT_STARTED,
  REQUEST_STARTED,
  REQUEST_IN_PROGRESS,
  REQUEST_COMPLETED,
  REQUEST_FAILED,
};

// Returns string representations of the request state.
std::string RequestTransferStateToString(RequestTransferState state);

// Structure that packs progress information of each request.
struct RequestProgressStatus {
  explicit RequestProgressStatus(const base::FilePath& file_path);

  RequestID request_id;

  // Drive path of the file dealt with the current request.
  base::FilePath file_path;
  // Current state of the transfer;
  RequestTransferState transfer_state;
};

// This class tracks all the in-flight Google API requests and manage
// their lifetime.
class RequestRegistry {
 public:
  RequestRegistry();
  ~RequestRegistry();

  // Base class for requests that this registry class can maintain.
  // NotifyStart() passes the ownership of the Request object to the registry.
  // In particular, calling NotifyFinish() causes the registry to delete the
  // Request object itself.
  class Request {
   public:
    explicit Request(RequestRegistry* registry);
    Request(RequestRegistry* registry, const base::FilePath& file_path);
    virtual ~Request();

    // Cancels the ongoing request. NotifyFinish() is called and the Request
    // object is deleted once the cancellation is done in DoCancel().
    void Cancel();

    // Retrieves the current progress status of the request.
    const RequestProgressStatus& progress_status() const {
      return progress_status_;
    }

   protected:
    // Notifies the registry about current status.
    void NotifyStart();
    void NotifyFinish(RequestTransferState status);

   private:
    // Does the cancellation.
    virtual void DoCancel() = 0;

    RequestRegistry* const registry_;
    RequestProgressStatus progress_status_;
  };

  // Cancels all in-flight requests.
  void CancelAll();

  // Cancels ongoing request for a given virtual |file_path|. Returns true if
  // the request was found and canceled.
  bool CancelForFilePath(const base::FilePath& file_path);

  // Cancels the specified request.
  void CancelRequest(Request* request);

 private:
  // Handlers for notifications from Requests.
  friend class Request;
  // Notifies that an request has started. This method passes the ownership of
  // the request to the registry. A fresh request ID is returned to *id.
  void OnRequestStart(Request* request, RequestID* id);
  void OnRequestFinish(RequestID request_id);

  typedef IDMap<Request, IDMapOwnPointer> RequestIDMap;
  RequestIDMap in_flight_requests_;

  DISALLOW_COPY_AND_ASSIGN(RequestRegistry);
};

}  // namespace google_apis

#endif  // CHROME_BROWSER_GOOGLE_APIS_REQUEST_REGISTRY_H_
