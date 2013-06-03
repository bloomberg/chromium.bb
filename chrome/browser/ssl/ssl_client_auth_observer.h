// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SSL_SSL_CLIENT_AUTH_OBSERVER_H_
#define CHROME_BROWSER_SSL_SSL_CLIENT_AUTH_OBSERVER_H_

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

namespace net {
class HttpNetworkSession;
class SSLCertRequestInfo;
class X509Certificate;
}

class SSLClientAuthObserver : public content::NotificationObserver {
 public:
  SSLClientAuthObserver(
      const net::HttpNetworkSession* network_session,
      net::SSLCertRequestInfo* cert_request_info,
      const base::Callback<void(net::X509Certificate*)>& callback);
  virtual ~SSLClientAuthObserver();

  // UI should implement this to close the dialog.
  virtual void OnCertSelectedByNotification() = 0;

  // Send content the certificate. Can also call with NULL if the user
  // cancelled. Derived classes must use this instead of caching the callback
  // and calling it directly.
  void CertificateSelected(net::X509Certificate* cert);

  // Begins observing notifications from other SSLClientAuthHandler instances.
  // If another instance chooses a cert for a matching SSLCertRequestInfo, we
  // will also use the same cert and OnCertSelectedByNotification will be called
  // so that the cert selection UI can be closed.
  void StartObserving();

  // Stops observing notifications.  We will no longer act on client auth
  // notifications.
  void StopObserving();

  net::SSLCertRequestInfo* cert_request_info() const {
    return cert_request_info_.get();
  }

 private:
  // content::NotificationObserver implementation:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  const net::HttpNetworkSession* network_session_;
  scoped_refptr<net::SSLCertRequestInfo> cert_request_info_;
  base::Callback<void(net::X509Certificate*)> callback_;
  content::NotificationRegistrar notification_registrar_;

  DISALLOW_COPY_AND_ASSIGN(SSLClientAuthObserver);
};

#endif  // CHROME_BROWSER_SSL_SSL_CLIENT_AUTH_OBSERVER_H_
