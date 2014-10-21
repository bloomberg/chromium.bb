// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_LOADER_CERTIFICATE_RESOURCE_HANDLER_H_
#define CONTENT_BROWSER_LOADER_CERTIFICATE_RESOURCE_HANDLER_H_

#include <string>
#include <utility>
#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "content/browser/loader/resource_handler.h"
#include "net/base/mime_util.h"

namespace net {
class IOBuffer;
class URLRequest;
class URLRequestStatus;
}  // namespace net

namespace content {

// This class handles certificate mime types such as:
// - "application/x-x509-user-cert"
// - "application/x-x509-ca-cert"
// - "application/x-pkcs12"
//
class CertificateResourceHandler : public ResourceHandler {
 public:
  explicit CertificateResourceHandler(net::URLRequest* request);
  ~CertificateResourceHandler() override;

  bool OnUploadProgress(uint64 position, uint64 size) override;

  // Not needed, as this event handler ought to be the final resource.
  bool OnRequestRedirected(const net::RedirectInfo& redirect_info,
                           ResourceResponse* resp,
                           bool* defer) override;

  // Check if this indeed an X509 cert.
  bool OnResponseStarted(ResourceResponse* resp, bool* defer) override;

  // Pass-through implementation.
  bool OnWillStart(const GURL& url, bool* defer) override;

  // Pass-through implementation.
  bool OnBeforeNetworkStart(const GURL& url, bool* defer) override;

  // Create a new buffer to store received data.
  bool OnWillRead(scoped_refptr<net::IOBuffer>* buf,
                  int* buf_size,
                  int min_size) override;

  // A read was completed, maybe allocate a new buffer for further data.
  bool OnReadCompleted(int bytes_read, bool* defer) override;

  // Done downloading the certificate.
  void OnResponseCompleted(const net::URLRequestStatus& urs,
                           const std::string& sec_info,
                           bool* defer) override;

  // N/A to cert downloading.
  void OnDataDownloaded(int bytes_downloaded) override;

 private:
  typedef std::vector<std::pair<scoped_refptr<net::IOBuffer>,
                                size_t> > ContentVector;

  void AssembleResource();

  size_t content_length_;
  ContentVector buffer_;
  scoped_refptr<net::IOBuffer> read_buffer_;
  scoped_refptr<net::IOBuffer> resource_buffer_;  // Downloaded certificate.
  net::CertificateMimeType cert_type_;
  DISALLOW_COPY_AND_ASSIGN(CertificateResourceHandler);
};

}  // namespace content

#endif  // CONTENT_BROWSER_LOADER_CERTIFICATE_RESOURCE_HANDLER_H_
