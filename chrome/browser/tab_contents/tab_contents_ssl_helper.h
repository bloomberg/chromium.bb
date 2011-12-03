// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_CONTENTS_TAB_CONTENTS_SSL_HELPER_H_
#define CHROME_BROWSER_TAB_CONTENTS_TAB_CONTENTS_SSL_HELPER_H_
#pragma once

#include <map>

#include "base/memory/linked_ptr.h"
#include "base/memory/ref_counted.h"
#include "content/browser/renderer_host/render_view_host_delegate.h"

class SSLAddCertHandler;
class SSLClientAuthHandler;
class TabContentsWrapper;

class TabContentsSSLHelper {
 public:
  explicit TabContentsSSLHelper(TabContentsWrapper* tab_contents);
  virtual ~TabContentsSSLHelper();

  // Called when |handler| encounters an error in verifying a received client
  // certificate. Note that, because CAs often will not send us intermediate
  // certificates, the verification we can do is minimal: we verify the
  // certificate is parseable, that we have the corresponding private key, and
  // that the certificate has not expired.
  void OnVerifyClientCertificateError(
      scoped_refptr<SSLAddCertHandler> handler, int error_code);

  // Called when |handler| requests the user's confirmation in adding a client
  // certificate.
  void AskToAddClientCertificate(
      scoped_refptr<SSLAddCertHandler> handler);

  // Called when |handler| successfully adds a client certificate.
  void OnAddClientCertificateSuccess(
      scoped_refptr<SSLAddCertHandler> handler);

  // Called when |handler| encounters an error adding a client certificate.
  void OnAddClientCertificateError(
      scoped_refptr<SSLAddCertHandler> handler, int error_code);

  // Called when |handler| has completed, so the delegate may release any state
  // accumulated.
  void OnAddClientCertificateFinished(
      scoped_refptr<SSLAddCertHandler> handler);

  // Displays a dialog for selecting a client certificate and returns it to
  // the |handler|.
  void ShowClientCertificateRequestDialog(
      scoped_refptr<SSLClientAuthHandler> handler);

 private:
  TabContentsWrapper* tab_contents_;

  class SSLAddCertData;
  std::map<int, linked_ptr<SSLAddCertData> > request_id_to_add_cert_data_;

  SSLAddCertData* GetAddCertData(SSLAddCertHandler* handler);

  DISALLOW_COPY_AND_ASSIGN(TabContentsSSLHelper);
};

#endif  // CHROME_BROWSER_TAB_CONTENTS_TAB_CONTENTS_SSL_HELPER_H_
