// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/ssl_client_auth_observer.h"

#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "chrome/browser/chrome_notification_types.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "net/cert/x509_certificate.h"
#include "net/ssl/ssl_cert_request_info.h"

using content::BrowserThread;

typedef std::pair<net::SSLCertRequestInfo*, net::X509Certificate*> CertDetails;

SSLClientAuthObserver::SSLClientAuthObserver(
    const net::HttpNetworkSession* network_session,
    net::SSLCertRequestInfo* cert_request_info,
    const base::Callback<void(net::X509Certificate*)>& callback)
    : network_session_(network_session),
      cert_request_info_(cert_request_info),
      callback_(callback) {
}

SSLClientAuthObserver::~SSLClientAuthObserver() {
}

void SSLClientAuthObserver::CertificateSelected(
    net::X509Certificate* certificate) {
  if (callback_.is_null())
    return;

  // Stop listening right away so we don't get our own notification.
  StopObserving();

  CertDetails details;
  details.first = cert_request_info_.get();
  details.second = certificate;
  content::NotificationService* service =
      content::NotificationService::current();
  service->Notify(chrome::NOTIFICATION_SSL_CLIENT_AUTH_CERT_SELECTED,
                  content::Source<net::HttpNetworkSession>(network_session_),
                  content::Details<CertDetails>(&details));

  callback_.Run(certificate);
  callback_.Reset();
}

void SSLClientAuthObserver::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  VLOG(1) << "SSLClientAuthObserver::Observe " << this;
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(type == chrome::NOTIFICATION_SSL_CLIENT_AUTH_CERT_SELECTED);

  CertDetails* cert_details = content::Details<CertDetails>(details).ptr();
  if (!cert_details->first->host_and_port.Equals(
           cert_request_info_->host_and_port))
    return;

  VLOG(1) << this << " got matching notification and selecting cert "
          << cert_details->second;
  StopObserving();
  callback_.Run(cert_details->second);
  callback_.Reset();
  OnCertSelectedByNotification();
}

void SSLClientAuthObserver::StartObserving() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  notification_registrar_.Add(
      this, chrome::NOTIFICATION_SSL_CLIENT_AUTH_CERT_SELECTED,
      content::Source<net::HttpNetworkSession>(network_session_));
}

void SSLClientAuthObserver::StopObserving() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  notification_registrar_.RemoveAll();
}
