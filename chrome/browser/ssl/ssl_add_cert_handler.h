// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SSL_SSL_ADD_CERT_HANDLER_H_
#define CHROME_BROWSER_SSL_SSL_ADD_CERT_HANDLER_H_

#include "base/basictypes.h"
#include "base/ref_counted.h"
#include "base/string16.h"

namespace net {
class X509Certificate;
}
class URLRequest;

// This class handles adding a newly-generated client cert. It ensures there's a
// private key for the cert, displays the cert to the user, and adds it upon
// user approval.
// It is self-owned and deletes itself when finished.
class SSLAddCertHandler : public base::RefCountedThreadSafe<SSLAddCertHandler> {
 public:
  SSLAddCertHandler(URLRequest* request, net::X509Certificate* cert);

  net::X509Certificate* cert()  { return cert_; }

  // The platform-specific code calls this when it's done, to clean up.
  // If |addCert| is true, the cert will be added to the CertDatabase.
  void Finished(bool add_cert);

 private:
  friend class base::RefCountedThreadSafe<SSLAddCertHandler>;

  // Runs the user interface. Called on the UI thread. Calls AskToAddCert.
  void RunUI();

  // Platform-specific code that asks the user whether to add the cert.
  // Called on the UI thread.
  void AskToAddCert();

  // Utility to display an error message in a dialog box.
  void ShowError(const string16& error);

  // The cert to add.
  scoped_refptr<net::X509Certificate> cert_;

  DISALLOW_COPY_AND_ASSIGN(SSLAddCertHandler);
};

#endif  // CHROME_BROWSER_SSL_SSL_ADD_CERT_HANDLER_H_
