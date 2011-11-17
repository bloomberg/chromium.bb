// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SSL_SSL_CLIENT_AUTH_HANDLER_H_
#define CONTENT_BROWSER_SSL_SSL_CLIENT_AUTH_HANDLER_H_
#pragma once

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "net/base/ssl_cert_request_info.h"

namespace net {
class HttpNetworkSession;
class URLRequest;
class X509Certificate;
}  // namespace net

// This class handles the approval and selection of a certificate for SSL client
// authentication by the user.
// It is self-owned and deletes itself when the UI reports the user selection or
// when the net::URLRequest is cancelled.
class CONTENT_EXPORT SSLClientAuthHandler
    : public base::RefCountedThreadSafe<
          SSLClientAuthHandler, content::BrowserThread::DeleteOnIOThread> {
 public:
  SSLClientAuthHandler(net::URLRequest* request,
                       net::SSLCertRequestInfo* cert_request_info);

  // Selects a certificate and resumes the URL request with that certificate.
  // Should only be called on the IO thread.
  void SelectCertificate();

  // Invoked when the request associated with this handler is cancelled.
  // Should only be called on the IO thread.
  void OnRequestCancelled();

  // Calls DoCertificateSelected on the I/O thread.
  // Called on the UI thread after the user has made a selection (which may
  // be long after DoSelectCertificate returns, if the UI is modeless/async.)
  void CertificateSelected(net::X509Certificate* cert);

  // Like CertificateSelected, but does not send SSL_CLIENT_AUTH_CERT_SELECTED
  // notification.  Used to avoid notification re-spamming when other
  // certificate selectors act on a notification matching the same host.
  virtual void CertificateSelectedNoNotify(net::X509Certificate* cert);

  // Returns the SSLCertRequestInfo for this handler.
  net::SSLCertRequestInfo* cert_request_info() { return cert_request_info_; }

  // Returns the session the URL request is associated with.
  const net::HttpNetworkSession* http_network_session() const {
    return http_network_session_;
  }

 protected:
  virtual ~SSLClientAuthHandler();

 private:
  friend class base::RefCountedThreadSafe<
      SSLClientAuthHandler, content::BrowserThread::DeleteOnIOThread>;
  friend class content::BrowserThread;
  friend class DeleteTask<SSLClientAuthHandler>;

  // Notifies that the user has selected a cert.
  // Called on the IO thread.
  void DoCertificateSelected(net::X509Certificate* cert);

  // Selects a client certificate on the UI thread.
  void DoSelectCertificate(int render_process_host_id,
                           int render_view_host_id);

  // The net::URLRequest that triggered this client auth.
  net::URLRequest* request_;

  // The HttpNetworkSession |request_| is associated with.
  const net::HttpNetworkSession* http_network_session_;

  // The certs to choose from.
  scoped_refptr<net::SSLCertRequestInfo> cert_request_info_;

  DISALLOW_COPY_AND_ASSIGN(SSLClientAuthHandler);
};

class CONTENT_EXPORT SSLClientAuthObserver
    : public content::NotificationObserver {
 public:
  SSLClientAuthObserver(net::SSLCertRequestInfo* cert_request_info,
                        SSLClientAuthHandler* handler);
  virtual ~SSLClientAuthObserver();

  // UI should implement this to close the dialog.
  virtual void OnCertSelectedByNotification() = 0;

  // content::NotificationObserver implementation:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Begins observing notifications from other SSLClientAuthHandler instances.
  // If another instance chooses a cert for a matching SSLCertRequestInfo, we
  // will also use the same cert and OnCertSelectedByNotification will be called
  // so that the cert selection UI can be closed.
  void StartObserving();

  // Stops observing notifications.  We will no longer act on client auth
  // notifications.
  void StopObserving();

 private:
  scoped_refptr<net::SSLCertRequestInfo> cert_request_info_;

  scoped_refptr<SSLClientAuthHandler> handler_;

  content::NotificationRegistrar notification_registrar_;

  DISALLOW_COPY_AND_ASSIGN(SSLClientAuthObserver);
};

#endif  // CONTENT_BROWSER_SSL_SSL_CLIENT_AUTH_HANDLER_H_
