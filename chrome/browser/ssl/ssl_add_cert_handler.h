// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SSL_SSL_ADD_CERT_HANDLER_H_
#define CHROME_BROWSER_SSL_SSL_ADD_CERT_HANDLER_H_
#pragma once

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"

namespace net {
class URLRequest;
class X509Certificate;
}

// This class handles adding a newly-generated client cert. It ensures there's a
// private key for the cert, displays the cert to the user, and adds it upon
// user approval.
// It is self-owned and deletes itself when finished.
class SSLAddCertHandler : public base::RefCountedThreadSafe<SSLAddCertHandler> {
 public:
  SSLAddCertHandler(net::URLRequest* request, net::X509Certificate* cert,
                    int render_process_host_id, int render_view_id);

  net::X509Certificate* cert()  { return cert_; }

  int network_request_id() const { return network_request_id_; }

  // The platform-specific code calls this when it's done, to clean up.
  // If |addCert| is true, the cert will be added to the CertDatabase.
  void Finished(bool add_cert);

 private:
  friend class base::RefCountedThreadSafe<SSLAddCertHandler>;
  virtual ~SSLAddCertHandler();

  // Runs the handler. Called on the IO thread.
  void Run();

  // Platform-specific code that asks the user whether to add the cert.
  // Called on the UI thread.
  void AskToAddCert();

  // The cert to add.
  scoped_refptr<net::X509Certificate> cert_;

  // The id of the request which started the process.
  int network_request_id_;
  // The id of the |RenderProcessHost| which started the download.
  int render_process_host_id_;
  // The id of the |RenderView| which started the download.
  int render_view_id_;

  DISALLOW_COPY_AND_ASSIGN(SSLAddCertHandler);
};

#endif  // CHROME_BROWSER_SSL_SSL_ADD_CERT_HANDLER_H_
