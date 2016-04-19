// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/ssl_client_auth_observer.h"

#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "chrome/browser/chrome_notification_types.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/client_certificate_delegate.h"
#include "content/public/browser/notification_service.h"
#include "net/cert/x509_certificate.h"
#include "net/ssl/ssl_cert_request_info.h"

using content::BrowserThread;

typedef std::pair<net::SSLCertRequestInfo*, net::X509Certificate*> CertDetails;

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
    net::X509Certificate* certificate) {
  if (!delegate_)
    return;

  // Stop listening now that the delegate has been resolved. This is also to
  // avoid getting a self-notification.
  StopObserving();

  CertDetails details;
  details.first = cert_request_info_.get();
  details.second = certificate;
  content::NotificationService* service =
      content::NotificationService::current();
  service->Notify(chrome::NOTIFICATION_SSL_CLIENT_AUTH_CERT_SELECTED,
                  content::Source<content::BrowserContext>(browser_context_),
                  content::Details<CertDetails>(&details));

  delegate_->ContinueWithCertificate(certificate);
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
  DCHECK(type == chrome::NOTIFICATION_SSL_CLIENT_AUTH_CERT_SELECTED);

  CertDetails* cert_details = content::Details<CertDetails>(details).ptr();
  if (!cert_details->first->host_and_port.Equals(
           cert_request_info_->host_and_port))
    return;

  DVLOG(1) << this << " got matching notification and selecting cert "
           << cert_details->second;
  StopObserving();
  delegate_->ContinueWithCertificate(cert_details->second);
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
