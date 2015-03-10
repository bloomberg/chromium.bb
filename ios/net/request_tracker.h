// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_NET_REQUEST_TRACKER_H_
#define IOS_NET_REQUEST_TRACKER_H_

#import <Foundation/Foundation.h>

#include "base/callback_forward.h"
#include "base/mac/scoped_nsobject.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"

namespace net {
class SSLInfo;
class URLRequest;
class URLRequestContext;
}

@class CRNForwardingNetworkClientFactory;
@class CRNSimpleNetworkClientFactory;
class GURL;

namespace net {

// RequestTracker can be used to observe the network requests and customize the
// behavior of the network stack with CRNForwardingNetworkClients.
// Each network request can be associated with a RequestTracker through the
// GetRequestTracker().
// RequestTracker requires a RequestTrackerFactory.
// The RequestTracker can be created on one thread and used on a different one.
class RequestTracker {
 public:
  enum CacheMode {
    CACHE_NORMAL,
    CACHE_RELOAD,
    CACHE_HISTORY,
    CACHE_BYPASS,
    CACHE_ONLY,
  };

  typedef base::Callback<void(bool)> SSLCallback;

  class RequestTrackerFactory {
   public:
    // Returns false if |request| is associated to an invalid tracker and should
    // be cancelled. In this case |tracker| is set to nullptr.
    // Returns true if |request| is associated with a valid tracker or if the
    // request is not associated to any tracker.
    virtual bool GetRequestTracker(NSURLRequest* request,
                                   base::WeakPtr<RequestTracker>* tracker) = 0;
  };

  // Sets the RequestTrackerFactory. The factory has to be set before the
  // GetRequestTracker() function can be called.
  // Does not take ownership of |factory|.
  static void SetRequestTrackerFactory(RequestTrackerFactory* factory);

  // Returns false if |request| is associated to an invalid tracker and should
  // be cancelled. In this case |tracker| is set to nullptr.
  // Returns true if |request| is associated with a valid tracker or if the
  // request is not associated to any tracker.
  // Internally calls the RequestTrackerFactory.
  static bool GetRequestTracker(NSURLRequest* request,
                                base::WeakPtr<RequestTracker>* tracker);

  RequestTracker();

  base::WeakPtr<RequestTracker> GetWeakPtr();

  // This function has to be called before using the tracker.
  virtual void Init();

  // Add a factory that may create network clients for requests going through
  // this tracker.
  void AddNetworkClientFactory(CRNForwardingNetworkClientFactory* factory);

  // Registers a factory with the class that will be added to all trackers.
  // Requests without associated trackers can add clients from these factories
  // using GlobalClientsHandlingAnyRequest().
  // Only |-clientHandlingAnyRequest| will be called on |factory|, the other
  // methods are not supported.
  static void AddGlobalNetworkClientFactory(
      CRNForwardingNetworkClientFactory* factory);

  // Gets the request context associated with the tracker.
  virtual URLRequestContext* GetRequestContext() = 0;

  // Network client generation methods. All of these four ClientsHandling...
  // methods return an array of CRNForwardingNetworkClient instances, according
  // to the CRNForwardingNetworkClientFactories added to the tracker. The array
  // may be empty. The caller is responsible for taking ownership of the clients
  // in the array.

  // Static method that returns clients that can handle any request, for use
  // in cases where a request isn't associated with any request_tracker.
  static NSArray* GlobalClientsHandlingAnyRequest();

  // Returns clients that can handle any request.
  NSArray* ClientsHandlingAnyRequest();
  // Returns clients that can handle |request|.
  NSArray* ClientsHandlingRequest(const URLRequest& request);
  // Returns clients that can handle |request| with |response|.
  NSArray* ClientsHandlingRequestAndResponse(const URLRequest& request,
                                             NSURLResponse* response);
  // Returns clients that can handle a redirect of |request| to |new_url| based
  // on |redirect_response|.
  NSArray* ClientsHandlingRedirect(const URLRequest& request,
                                   const GURL& new_url,
                                   NSURLResponse* redirect_response);

  // Informs the tracker that a request has started.
  virtual void StartRequest(URLRequest* request) = 0;

  // Informs the tracker that the headers for the request are available.
  virtual void CaptureHeaders(URLRequest* request) = 0;

  // Informs the tracker the expected length of the result, if known.
  virtual void CaptureExpectedLength(const URLRequest* request,
                                     uint64_t length) = 0;

  // Informs the tracker that a request received par_trackert of its data.
  virtual void CaptureReceivedBytes(const URLRequest* request,
                                    uint64_t byte_count) = 0;

  // Informs the tracker that a certificate has been used.
  virtual void CaptureCertificatePolicyCache(
      const URLRequest* request,
      const SSLCallback& should_continue) = 0;

  // Notifies of the completion of a request. Success or failure.
  virtual void StopRequest(URLRequest* request) = 0;

  // Special case for a redirect as we fully expect another request to follow
  // very shortly.
  virtual void StopRedirectedRequest(URLRequest* request) = 0;

  // Called when there is an issue on the SSL certificate. The user must be
  // informed and if |recoverable| is YES the user decision to continue or not
  // will be send back via the |callback|. The callback must be safe to call
  // from any thread. If recoverable is NO, invoking the callback should be a
  // noop.
  virtual void OnSSLCertificateError(const URLRequest* request,
                                     const SSLInfo& ssl_info,
                                     bool recoverable,
                                     const SSLCallback& should_continue) = 0;

  // Gets and sets the cache mode.
  CacheMode GetCacheMode() const;
  void SetCacheMode(RequestTracker::CacheMode mode);

 protected:
  virtual ~RequestTracker();

  void InvalidateWeakPtrs();

 private:
  // Array of client factories that may be added by CRNHTTPProtocolHandler. The
  // array lives on the IO thread.
  base::scoped_nsobject<NSMutableArray> client_factories_;

  bool initialized_;
  CacheMode cache_mode_;
  base::ThreadChecker thread_checker_;

  base::WeakPtrFactory<RequestTracker> weak_ptr_factory_;
};

}  // namespace net

#endif  // IOS_NET_REQUEST_TRACKER_H_
