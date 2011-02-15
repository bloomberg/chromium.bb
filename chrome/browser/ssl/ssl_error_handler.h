// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SSL_SSL_ERROR_HANDLER_H_
#define CHROME_BROWSER_SSL_SSL_ERROR_HANDLER_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/ref_counted.h"
#include "chrome/browser/renderer_host/global_request_id.h"
#include "chrome/browser/ssl/ssl_manager.h"
#include "googleurl/src/gurl.h"
#include "webkit/glue/resource_type.h"

class ResourceDispatcherHost;
class SSLCertErrorHandler;
class TabContents;

namespace net {
class URLRequest;
}  // namespace net

// An SSLErrorHandler carries information from the IO thread to the UI thread
// and is dispatched to the appropriate SSLManager when it arrives on the
// UI thread.  Subclasses should override the OnDispatched/OnDispatchFailed
// methods to implement the actions that should be taken on the UI thread.
// These methods can call the different convenience methods ContinueRequest/
// CancelRequest to perform any required action on the net::URLRequest the
// ErrorHandler was created with.
//
// IMPORTANT NOTE:
//
//   If you are not doing anything in OnDispatched/OnDispatchFailed, make sure
//   you call TakeNoAction().  This is necessary for ensuring the instance is
//   not leaked.
//
class SSLErrorHandler : public base::RefCountedThreadSafe<SSLErrorHandler> {
 public:
  virtual SSLCertErrorHandler* AsSSLCertErrorHandler();

  // Find the appropriate SSLManager for the net::URLRequest and begin handling
  // this error.
  //
  // Call on UI thread.
  void Dispatch();

  // Available on either thread.
  const GURL& request_url() const { return request_url_; }

  // Available on either thread.
  ResourceType::Type resource_type() const { return resource_type_; }

  // Returns the TabContents this object is associated with.  Should be
  // called from the UI thread.
  TabContents* GetTabContents();

  // Cancels the associated net::URLRequest.
  // This method can be called from OnDispatchFailed and OnDispatched.
  void CancelRequest();

  // Continue the net::URLRequest ignoring any previous errors.  Note that some
  // errors cannot be ignored, in which case this will result in the request
  // being canceled.
  // This method can be called from OnDispatchFailed and OnDispatched.
  void ContinueRequest();

  // Cancels the associated net::URLRequest and mark it as denied.  The renderer
  // processes such request in a special manner, optionally replacing them
  // with alternate content (typically frames content is replaced with a
  // warning message).
  // This method can be called from OnDispatchFailed and OnDispatched.
  void DenyRequest();

  // Does nothing on the net::URLRequest but ensures the current instance ref
  // count is decremented appropriately.  Subclasses that do not want to
  // take any specific actions in their OnDispatched/OnDispatchFailed should
  // call this.
  void TakeNoAction();

 protected:
  friend class base::RefCountedThreadSafe<SSLErrorHandler>;

  // Construct on the IO thread.
  SSLErrorHandler(ResourceDispatcherHost* resource_dispatcher_host,
                  net::URLRequest* request,
                  ResourceType::Type resource_type);

  virtual ~SSLErrorHandler();

  // The following 2 methods are the methods subclasses should implement.
  virtual void OnDispatchFailed();

  // Can use the manager_ member.
  virtual void OnDispatched();

  // Should only be accessed on the UI thread.
  SSLManager* manager_;  // Our manager.

  // The id of the net::URLRequest associated with this object.
  // Should only be accessed from the IO thread.
  GlobalRequestID request_id_;

  // The ResourceDispatcherHost we are associated with.
  ResourceDispatcherHost* resource_dispatcher_host_;

 private:
  // Completes the CancelRequest operation on the IO thread.
  // Call on the IO thread.
  void CompleteCancelRequest(int error);

  // Completes the ContinueRequest operation on the IO thread.
  //
  // Call on the IO thread.
  void CompleteContinueRequest();

  // Derefs this instance.
  // Call on the IO thread.
  void CompleteTakeNoAction();

  // We use these members to find the correct SSLManager when we arrive on
  // the UI thread.
  int render_process_host_id_;
  int tab_contents_id_;

  // The URL that we requested.
  // This read-only member can be accessed on any thread.
  const GURL request_url_;

  // What kind of resource is associated with the requested that generated
  // that error.
  // This read-only member can be accessed on any thread.
  const ResourceType::Type resource_type_;

  // A flag to make sure we notify the net::URLRequest exactly once.
  // Should only be accessed on the IO thread
  bool request_has_been_notified_;

  DISALLOW_COPY_AND_ASSIGN(SSLErrorHandler);
};

#endif  // CHROME_BROWSER_SSL_SSL_ERROR_HANDLER_H_
