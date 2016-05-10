// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SSL_CLIENT_CERTIFICATE_SELECTOR_H_
#define CHROME_BROWSER_UI_VIEWS_SSL_CLIENT_CERTIFICATE_SELECTOR_H_

#include "base/macros.h"
#include "chrome/browser/ssl/ssl_client_auth_observer.h"
#include "chrome/browser/ssl/ssl_client_certificate_selector.h"
#include "chrome/browser/ui/views/certificate_selector.h"
#include "content/public/browser/web_contents_observer.h"

// This header file exists only for testing.  Chrome should access the
// certificate selector only through the cross-platform interface
// chrome/browser/ssl_client_certificate_selector.h.

namespace content {
class WebContents;
}

namespace net {
class SSLCertRequestInfo;
class X509Certificate;
}

class SSLClientCertificateSelector : public chrome::CertificateSelector,
                                     public SSLClientAuthObserver,
                                     public content::WebContentsObserver {
 public:
  SSLClientCertificateSelector(
      content::WebContents* web_contents,
      const scoped_refptr<net::SSLCertRequestInfo>& cert_request_info,
      std::unique_ptr<content::ClientCertificateDelegate> delegate);
  ~SSLClientCertificateSelector() override;

  void Init();

  // SSLClientAuthObserver:
  void OnCertSelectedByNotification() override;

  // chrome::CertificateSelector:
  void DeleteDelegate() override;
  bool Accept() override;

  // content::WebContentsObserver:
  void WebContentsDestroyed() override;

 private:
  // Callback after unlocking certificate slot.
  void Unlocked(net::X509Certificate* cert);

  DISALLOW_COPY_AND_ASSIGN(SSLClientCertificateSelector);
};

#endif  // CHROME_BROWSER_UI_VIEWS_SSL_CLIENT_CERTIFICATE_SELECTOR_H_
