// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/ssl_client_auth_observer.h"

#include <tuple>

#include "base/bind.h"
#include "base/logging.h"
#include "chrome/browser/chrome_notification_types.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/client_certificate_delegate.h"
#include "content/public/browser/notification_service.h"
#include "net/cert/x509_certificate.h"
#include "net/ssl/ssl_cert_request_info.h"
#include "net/ssl/ssl_private_key.h"

using content::BrowserThread;

using CertDetails = std::
    tuple<net::SSLCertRequestInfo*, net::X509Certificate*, net::SSLPrivateKey*>;

SSLClientAuthObserver::SSLClientAuthObserver(
    const content::BrowserContext* browser_context,
    const scoped_refptr<net::SSLCertRequestInfo>& cert_request_info,
    std::unique_ptr<content::ClientCertificateDelegate> delegate)
    : browser_context_(browser_context),
      cert_request_info_(cert_request_info),
      delegate_(std::move(delegate)) {}

SSLClientAuthObserver::~SSLClientAuthObserver() {
}

void SSLClientAuthObserver::CertificateSelected(
    net::X509Certificate* certificate,
    net::SSLPrivateKey* private_key) {
  if (!delegate_)
    return;

  // Stop listening now that the delegate has been resolved. This is also to
  // avoid getting a self-notification.
  StopObserving();

  CertDetails details(cert_request_info_.get(), certificate, private_key);
  content::NotificationService* service =
      content::NotificationService::current();
  service->Notify(chrome::NOTIFICATION_SSL_CLIENT_AUTH_CERT_SELECTED,
                  content::Source<content::BrowserContext>(browser_context_),
                  content::Details<CertDetails>(&details));

  delegate_->ContinueWithCertificate(certificate, private_key);
  delegate_.reset();
}

void SSLClientAuthObserver::CancelCertificateSelection() {
  if (!delegate_)
    return;

  // Stop observing now that the delegate has been resolved.
  StopObserving();
  delegate_.reset();
}

void SSLClientAuthObserver::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DVLOG(1) << "SSLClientAuthObserver::Observe " << this;
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_EQ(chrome::NOTIFICATION_SSL_CLIENT_AUTH_CERT_SELECTED, type);

  CertDetails* cert_details = content::Details<CertDetails>(details).ptr();
  if (!std::get<0>(*cert_details)
           ->host_and_port.Equals(cert_request_info_->host_and_port))
    return;

  DVLOG(1) << this << " got matching notification and selecting cert "
           << std::get<1>(*cert_details);
  StopObserving();
  delegate_->ContinueWithCertificate(std::get<1>(*cert_details),
                                     std::get<2>(*cert_details));
  delegate_.reset();
  OnCertSelectedByNotification();
}

void SSLClientAuthObserver::StartObserving() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  notification_registrar_.Add(
      this, chrome::NOTIFICATION_SSL_CLIENT_AUTH_CERT_SELECTED,
      content::Source<content::BrowserContext>(browser_context_));
}

void SSLClientAuthObserver::StopObserving() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  notification_registrar_.RemoveAll();
}
