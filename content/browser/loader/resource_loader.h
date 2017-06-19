// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_LOADER_RESOURCE_LOADER_H_
#define CONTENT_BROWSER_LOADER_RESOURCE_LOADER_H_

#include <memory>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "content/browser/loader/resource_controller.h"
#include "content/browser/loader/resource_handler.h"
#include "content/browser/ssl/ssl_client_auth_handler.h"
#include "content/browser/ssl/ssl_error_handler.h"
#include "content/common/content_export.h"
#include "net/url_request/url_request.h"
#include "url/gurl.h"

namespace net {
class X509Certificate;
}

namespace content {
class ResourceDispatcherHostLoginDelegate;
class ResourceHandler;
class ResourceLoaderDelegate;
class ResourceRequestInfoImpl;

// This class is responsible for driving the URLRequest (i.e., calling Start,
// Read, and servicing events).  It has a ResourceHandler, which is typically a
// chain of ResourceHandlers, and is the ResourceController for its handler.
class CONTENT_EXPORT ResourceLoader : public net::URLRequest::Delegate,
                                      public SSLErrorHandler::Delegate,
                                      public SSLClientAuthHandler::Delegate,
                                      public ResourceHandler::Delegate {
 public:
  ResourceLoader(std::unique_ptr<net::URLRequest> request,
                 std::unique_ptr<ResourceHandler> handler,
                 ResourceLoaderDelegate* delegate);
  ~ResourceLoader() override;

  void StartRequest();
  void CancelRequest(bool from_renderer);

  bool is_transferring() const { return is_transferring_; }
  void MarkAsTransferring(const base::Closure& on_transfer_complete_callback);
  void CompleteTransfer();

  net::URLRequest* request() { return request_.get(); }
  ResourceRequestInfoImpl* GetRequestInfo();

  void ClearLoginDelegate();

  // ResourceHandler::Delegate implementation:
  void OutOfBandCancel(int error_code, bool tell_renderer) override;

 private:
  // ResourceController implementation for the ResourceLoader.
  class Controller;

  // net::URLRequest::Delegate implementation:
  void OnReceivedRedirect(net::URLRequest* request,
                          const net::RedirectInfo& redirect_info,
                          bool* defer) override;
  void OnAuthRequired(net::URLRequest* request,
                      net::AuthChallengeInfo* info) override;
  void OnCertificateRequested(net::URLRequest* request,
                              net::SSLCertRequestInfo* info) override;
  void OnSSLCertificateError(net::URLRequest* request,
                             const net::SSLInfo& info,
                             bool fatal) override;
  void OnResponseStarted(net::URLRequest* request) override;
  void OnReadCompleted(net::URLRequest* request, int bytes_read) override;

  // SSLErrorHandler::Delegate implementation:
  void CancelSSLRequest(int error, const net::SSLInfo* ssl_info) override;
  void ContinueSSLRequest() override;

  // SSLClientAuthHandler::Delegate implementation.
  void ContinueWithCertificate(
      scoped_refptr<net::X509Certificate> cert,
      scoped_refptr<net::SSLPrivateKey> private_key) override;
  void CancelCertificateSelection() override;

  // These correspond to Controller's methods.
  // TODO(mmenke):  Seems like this could be simplified a little.

  // |called_from_resource_controller| is true if called directly from a
  // ResourceController, in which case |resource_handler_| must not be invoked
  // or destroyed synchronously to avoid re-entrancy issues, and false
  // otherwise.
  void Resume(bool called_from_resource_controller);
  void Cancel();
  void CancelAndIgnore();
  void CancelWithError(int error_code);

  void StartRequestInternal();
  void CancelRequestInternal(int error, bool from_renderer);
  void FollowDeferredRedirectInternal();
  void CompleteResponseStarted();
  // If |handle_result_async| is true, the result of the following read will be
  // handled asynchronously if it completes synchronously, unless it's EOF or an
  // error. This is to prevent a single request from blocking the thread for too
  // long.
  void PrepareToReadMore(bool handle_result_async);
  void ReadMore(bool handle_result_async);
  void ResumeReading();
  // Passes a read result to the handler.
  void CompleteRead(int bytes_read);
  void ResponseCompleted();
  void CallDidFinishLoading();
  void RecordHistograms();

  bool is_deferred() const { return deferred_stage_ != DEFERRED_NONE; }

  // Used for categorizing loading of prefetches for reporting in histograms.
  // NOTE: This enumeration is used in histograms, so please do not add entries
  // in the middle.
  enum PrefetchStatus {
    STATUS_UNDEFINED,
    STATUS_SUCCESS_FROM_CACHE,
    STATUS_SUCCESS_FROM_NETWORK,
    STATUS_CANCELED,
    STATUS_SUCCESS_ALREADY_PREFETCHED,
    STATUS_MAX,
  };

  enum DeferredStage {
    DEFERRED_NONE,
    // Magic deferral "stage" which means that the code is currently in a
    // recursive call from the ResourceLoader. When in this state, Resume() does
    // nothing but update the deferral state, and when the stack is unwound back
    // up to the ResourceLoader, the request will be continued. This is used to
    // prevent the stack from getting too deep.
    DEFERRED_SYNC,
    DEFERRED_START,
    DEFERRED_REDIRECT,
    DEFERRED_ON_WILL_READ,
    DEFERRED_READ,
    DEFERRED_RESPONSE_COMPLETE,
    DEFERRED_FINISH
  };
  DeferredStage deferred_stage_;

  class ScopedDeferral;

  std::unique_ptr<net::URLRequest> request_;
  std::unique_ptr<ResourceHandler> handler_;
  ResourceLoaderDelegate* delegate_;

  scoped_refptr<ResourceDispatcherHostLoginDelegate> login_delegate_;
  std::unique_ptr<SSLClientAuthHandler> ssl_client_auth_handler_;

  base::TimeTicks read_deferral_start_time_;

  // Indicates that we are in a state of being transferred to a new downstream
  // consumer.  We are waiting for a notification to complete the transfer, at
  // which point we'll receive a new ResourceHandler.
  bool is_transferring_;

  // Called when a navigation has finished transfer.
  base::Closure on_transfer_complete_callback_;

  // Instrumentation add to investigate http://crbug.com/503306.
  // TODO(mmenke): Remove once bug is fixed.
  int times_cancelled_before_request_start_;
  bool started_request_;
  int times_cancelled_after_request_start_;

  // Stores the URL from a deferred redirect.
  GURL deferred_redirect_url_;

  // Read buffer and its size.  Class members as OnWillRead can complete
  // asynchronously.
  scoped_refptr<net::IOBuffer> read_buffer_;
  int read_buffer_size_;

  base::ThreadChecker thread_checker_;

  base::WeakPtrFactory<ResourceLoader> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ResourceLoader);
};

}  // namespace content

#endif  // CONTENT_BROWSER_LOADER_RESOURCE_LOADER_H_
