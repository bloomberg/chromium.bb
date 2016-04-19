// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SSL_SSL_CLIENT_AUTH_OBSERVER_H_
#define CHROME_BROWSER_SSL_SSL_CLIENT_AUTH_OBSERVER_H_

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

namespace net {
class SSLCertRequestInfo;
class X509Certificate;
}

namespace content {
class BrowserContext;
class ClientCertificateDelegate;
}

// SSLClientAuthObserver is a base class that wraps a
// ClientCertificateDelegate. It links client certificate selection dialogs
// attached to the same BrowserContext. When CertificateSelected is called via
// one of them, the rest simulate the same action.
class SSLClientAuthObserver : public content::NotificationObserver {
 public:
  SSLClientAuthObserver(
      const content::BrowserContext* browser_context,
      const scoped_refptr<net::SSLCertRequestInfo>& cert_request_info,
      std::unique_ptr<content::ClientCertificateDelegate> delegate);
  ~SSLClientAuthObserver() override;

  // UI should implement this to close the dialog.
  virtual void OnCertSelectedByNotification() = 0;

  // Continues the request with a certificate. Can also call with NULL to
  // continue with no certificate. Derived classes must use this instead of
  // caching the delegate and calling it directly.
  void CertificateSelected(net::X509Certificate* cert);

  // Cancels the certificate selection and aborts the request.
  void CancelCertificateSelection();

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
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  const content::BrowserContext* browser_context_;
  scoped_refptr<net::SSLCertRequestInfo> cert_request_info_;
  std::unique_ptr<content::ClientCertificateDelegate> delegate_;
  content::NotificationRegistrar notification_registrar_;

  DISALLOW_COPY_AND_ASSIGN(SSLClientAuthObserver);
};

#endif  // CHROME_BROWSER_SSL_SSL_CLIENT_AUTH_OBSERVER_H_
