// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_NET_CONNECTIVITY_CHECKER_H_
#define CHROMECAST_NET_CONNECTIVITY_CHECKER_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "net/base/network_change_notifier.h"
#include "net/url_request/url_request.h"

class GURL;

namespace base {
class MessageLoopProxy;
}

namespace net {
class URLRequestContext;
}

namespace chromecast {

// Simple class to check network connectivity by sending a HEAD http request
// to given url.
class ConnectivityChecker
    : public base::RefCountedThreadSafe<ConnectivityChecker>,
      public net::URLRequest::Delegate,
      public net::NetworkChangeNotifier::ConnectionTypeObserver,
      public net::NetworkChangeNotifier::IPAddressObserver {
 public:
  class ConnectivityObserver {
   public:
    // Will be called when internet connectivity changes
    virtual void OnConnectivityChanged(bool connected) = 0;

   protected:
    ConnectivityObserver() {}
    virtual ~ConnectivityObserver() {}

   private:
    DISALLOW_COPY_AND_ASSIGN(ConnectivityObserver);
  };

  explicit ConnectivityChecker(
      const scoped_refptr<base::MessageLoopProxy>& loop_proxy);

  void AddConnectivityObserver(ConnectivityObserver* observer);
  void RemoveConnectivityObserver(ConnectivityObserver* observer);

  // Returns if there is internet connectivity
  bool Connected() const;

  // Checks for connectivity
  void Check();

 protected:
  ~ConnectivityChecker() override;

 private:
  friend class base::RefCountedThreadSafe<ConnectivityChecker>;

  // UrlRequest::Delegate implementation:
  void OnResponseStarted(net::URLRequest* request) override;
  void OnReadCompleted(net::URLRequest* request, int bytes_read) override;

  // Initializes ConnectivityChecker
  void Initialize();

  // NetworkChangeNotifier::ConnectionTypeObserver implementation:
  void OnConnectionTypeChanged(
      net::NetworkChangeNotifier::ConnectionType type) override;

  // net::NetworkChangeNotifier::IPAddressObserver implementation:
  void OnIPAddressChanged() override;

  // Cancels current connectivity checking in progress.
  void Cancel();

  // Sets connectivity and alerts observers if it has changed
  void SetConnectivity(bool connected);

  scoped_ptr<GURL> connectivity_check_url_;
  scoped_ptr<net::URLRequestContext> url_request_context_;
  scoped_ptr<net::URLRequest> url_request_;
  const scoped_refptr<ObserverListThreadSafe<ConnectivityObserver> >
      connectivity_observer_list_;
  const scoped_refptr<base::MessageLoopProxy> loop_proxy_;
  bool connected_;
  unsigned int bad_responses_;

  DISALLOW_COPY_AND_ASSIGN(ConnectivityChecker);
};

}  // namespace chromecast

#endif  // CHROMECAST_NET_CONNECTIVITY_CHECKER_H_
