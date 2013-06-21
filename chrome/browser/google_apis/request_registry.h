// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GOOGLE_APIS_REQUEST_REGISTRY_H_
#define CHROME_BROWSER_GOOGLE_APIS_REQUEST_REGISTRY_H_

#include "base/basictypes.h"
#include "base/id_map.h"

namespace google_apis {

// Unique ID to identify each request.
typedef int32 RequestID;

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
    virtual ~Request();

   protected:
    // Notifies the registry about current status.
    void NotifyStart();
    void NotifyFinish();

   private:
    RequestRegistry* const registry_;
    RequestID id_;
  };

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
