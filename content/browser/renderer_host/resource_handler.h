// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is the browser side of the resource dispatcher, it receives requests
// from the RenderProcessHosts, and dispatches them to URLRequests. It then
// fowards the messages from the URLRequests back to the correct process for
// handling.
//
// See http://dev.chromium.org/developers/design-documents/multi-process-resource-loading

#ifndef CONTENT_BROWSER_RENDERER_HOST_RESOURCE_HANDLER_H_
#define CONTENT_BROWSER_RENDERER_HOST_RESOURCE_HANDLER_H_
#pragma once

#include <string>

#include "chrome/browser/browser_thread.h"

namespace net {
class IOBuffer;
class URLRequestStatus;
}  // namespace net

struct ResourceResponse;
class GURL;

// The resource dispatcher host uses this interface to push load events to the
// renderer, allowing for differences in the types of IPC messages generated.
// See the implementations of this interface defined below.
class ResourceHandler
    : public base::RefCountedThreadSafe<
          ResourceHandler, BrowserThread::DeleteOnIOThread> {
 public:
  // Called as upload progress is made.
  virtual bool OnUploadProgress(int request_id,
                                uint64 position,
                                uint64 size) = 0;

  // The request was redirected to a new URL.  |*defer| has an initial value of
  // false.  Set |*defer| to true to defer the redirect.  The redirect may be
  // followed later on via ResourceDispatcherHost::FollowDeferredRedirect.
  virtual bool OnRequestRedirected(int request_id, const GURL& url,
                                   ResourceResponse* response,
                                   bool* defer) = 0;

  // Response headers and meta data are available.
  virtual bool OnResponseStarted(int request_id,
                                 ResourceResponse* response) = 0;

  // Called before the net::URLRequest for |request_id| (whose url is |url|) is
  // to be started. If the handler returns false, then the request is cancelled.
  // Otherwise if the return value is true, the ResourceHandler can delay the
  // request from starting by setting |*defer = true|. A deferred request will
  // not have called net::URLRequest::Start(), and will not resume until someone
  // calls ResourceDispatcherHost::StartDeferredRequest().
  virtual bool OnWillStart(int request_id, const GURL& url, bool* defer) = 0;

  // Data will be read for the response.  Upon success, this method places the
  // size and address of the buffer where the data is to be written in its
  // out-params.  This call will be followed by either OnReadCompleted or
  // OnResponseCompleted, at which point the buffer may be recycled.
  virtual bool OnWillRead(int request_id,
                          net::IOBuffer** buf,
                          int* buf_size,
                          int min_size) = 0;

  // Data (*bytes_read bytes) was written into the buffer provided by
  // OnWillRead. A return value of false cancels the request, true continues
  // reading data.
  virtual bool OnReadCompleted(int request_id, int* bytes_read) = 0;

  // The response is complete.  The final response status is given.
  // Returns false if the handler is deferring the call to a later time.
  virtual bool OnResponseCompleted(int request_id,
                                   const net::URLRequestStatus& status,
                                   const std::string& security_info) = 0;

  // Signals that the request is closed (i.e. finished successfully, cancelled).
  // This is a signal that the associated net::URLRequest isn't valid anymore.
  virtual void OnRequestClosed() = 0;

  // This notification is synthesized by the RedirectToFileResourceHandler
  // to indicate progress of 'download_to_file' requests. OnReadCompleted
  // calls are consumed by the RedirectToFileResourceHandler and replaced
  // with OnDataDownloaded calls.
  virtual void OnDataDownloaded(int request_id, int bytes_downloaded) {}

 protected:
  friend class BrowserThread;
  friend class DeleteTask<ResourceHandler>;

  virtual ~ResourceHandler() {}
};

#endif  // CONTENT_BROWSER_RENDERER_HOST_RESOURCE_HANDLER_H_
