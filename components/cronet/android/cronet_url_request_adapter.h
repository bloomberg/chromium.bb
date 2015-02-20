// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRONET_ANDROID_CRONET_URL_REQUEST_ADAPTER_H_
#define COMPONENTS_CRONET_ANDROID_CRONET_URL_REQUEST_ADAPTER_H_

#include <string>

#include "base/callback.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "net/base/request_priority.h"
#include "net/http/http_request_headers.h"
#include "net/url_request/url_request.h"
#include "url/gurl.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace net {
class GrowableIOBuffer;
class HttpResponseHeaders;
class UploadDataStream;
}  // namespace net

namespace cronet {

class CronetURLRequestContextAdapter;

// An adapter from the JNI CronetUrlRequest object to the Chromium URLRequest.
// Created and configured from random thread. Start is posted to network
// thread and all callbacks into CronetURLRequestAdapterDelegate are done on
// network thread. Each delegate callback is expected to initiate next step like
// FollowDeferredRedirect, ReadData or Destroy. All methods except those needed
// to set up a request must be called exclusively on the network thread.
class CronetURLRequestAdapter : public net::URLRequest::Delegate {
 public:
  // The delegate which is called when the request adapter completes a step.
  // Always called on network thread.
  class CronetURLRequestAdapterDelegate {
   public:
    // Called on redirect. Consumer should either destroy the request, or
    // call FollowDeferredRedirect.
    virtual void OnRedirect(const GURL& newLocation, int http_status_code) = 0;
    // Called when response has started and headers have been received. Consumer
    // should either destroy the request, or call ReadData.
    virtual void OnResponseStarted(int http_status_code) = 0;
    // Called when response bytes were read. Consumer should consume data and
    // either destroy the request, or call ReadData. The request may only be
    // destroyed after the embedder is done with |bytes|, as deleting the
    // request frees the buffer.
    virtual void OnBytesRead(unsigned char* bytes,
                             int bytes_read) = 0;
    // Called when response has finished successfully. Consumer should destroy
    // the request.
    virtual void OnRequestFinished() = 0;
    // Called when response has finished with error. Consumer should destroy
    // the request.
    virtual void OnError(int net_error) = 0;

    virtual ~CronetURLRequestAdapterDelegate() {}
  };

  CronetURLRequestAdapter(
      CronetURLRequestContextAdapter* context,
      scoped_ptr<CronetURLRequestAdapterDelegate> delegate,
      const GURL& url,
      net::RequestPriority priority);
  ~CronetURLRequestAdapter() override;

  // Methods called prior to Start are never called on network thread.

  // Sets the request method GET, POST etc.
  void set_method(const std::string& method) {
    initial_method_ = method;
  }

  // Adds a header to the request before it starts.
  void AddRequestHeader(const std::string& name, const std::string& value);

  // Adds a request body to the request before it starts.
  void SetUpload(scoped_ptr<net::UploadDataStream> upload);

  // Methods called on any thread.

  // Posts tasks to network thread.
  void PostTaskToNetworkThread(const tracked_objects::Location& from_here,
                               const base::Closure& task);

  // Returns true if called on network thread.
  bool IsOnNetworkThread() const;

  // Methods called only on network thread.

  // Starts the request.
  void Start();

  // Follows redirect.
  void FollowDeferredRedirect();

  // Reads more data.
  void ReadData();

  // Releases all resources for the request and deletes the object itself.
  void Destroy();

  // When called during a OnRedirect or OnResponseStarted callback, these
  // methods return the corresponding response information.

  // Gets all response headers, as a HttpResponseHeaders object. Returns NULL
  // if the last attempted request received no headers.
  const net::HttpResponseHeaders* GetResponseHeaders() const;

  // Gets NPN or ALPN Negotiated Protocol (if any) from HttpResponseInfo.
  const std::string& GetNegotiatedProtocol() const;

  // Returns true if response is coming from the cache.
  bool GetWasCached() const;

  // Gets the total amount of body data received from network after
  // SSL/SPDY/QUIC decoding and proxy handling.  Basically the size of the body
  // prior to decompression.
  int64 GetTotalReceivedBytes() const;

  // Bypasses cache. If context is not set up to use cache, this call has no
  // effect.
  void DisableCache();

  // net::URLRequest::Delegate overrides.
  void OnReceivedRedirect(net::URLRequest* request,
                          const net::RedirectInfo& redirect_info,
                          bool* defer_redirect) override;
  void OnResponseStarted(net::URLRequest* request) override;
  void OnReadCompleted(net::URLRequest* request, int bytes_read) override;

 private:
  // Checks status of the request_adapter, return false if |is_success()| is
  // true, otherwise report error and cancel request_adapter.
  bool MaybeReportError(net::URLRequest* request) const;

  CronetURLRequestContextAdapter* context_;
  scoped_ptr<CronetURLRequestAdapterDelegate> delegate_;

  const GURL initial_url_;
  const net::RequestPriority initial_priority_;
  std::string initial_method_;
  int load_flags_;
  net::HttpRequestHeaders initial_request_headers_;
  scoped_ptr<net::UploadDataStream> upload_;

  scoped_refptr<net::IOBufferWithSize> read_buffer_;
  scoped_ptr<net::URLRequest> url_request_;

  DISALLOW_COPY_AND_ASSIGN(CronetURLRequestAdapter);
};

}  // namespace cronet

#endif  // COMPONENTS_CRONET_ANDROID_CRONET_URL_REQUEST_ADAPTER_H_
