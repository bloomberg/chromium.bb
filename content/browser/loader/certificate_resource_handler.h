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
#include "googleurl/src/gurl.h"
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
  CertificateResourceHandler(net::URLRequest* request,
                             int render_process_host_id,
                             int render_view_id);
  virtual ~CertificateResourceHandler();

  virtual bool OnUploadProgress(int request_id,
                                uint64 position,
                                uint64 size) OVERRIDE;

  // Not needed, as this event handler ought to be the final resource.
  virtual bool OnRequestRedirected(int request_id,
                                   const GURL& url,
                                   ResourceResponse* resp,
                                   bool* defer) OVERRIDE;

  // Check if this indeed an X509 cert.
  virtual bool OnResponseStarted(int request_id,
                                 ResourceResponse* resp,
                                 bool* defer) OVERRIDE;

  // Pass-through implementation.
  virtual bool OnWillStart(int request_id,
                           const GURL& url,
                           bool* defer) OVERRIDE;

  // Create a new buffer to store received data.
  virtual bool OnWillRead(int request_id,
                          net::IOBuffer** buf,
                          int* buf_size,
                          int min_size) OVERRIDE;

  // A read was completed, maybe allocate a new buffer for further data.
  virtual bool OnReadCompleted(int request_id,
                               int bytes_read,
                               bool* defer) OVERRIDE;

  // Done downloading the certificate.
  virtual bool OnResponseCompleted(int request_id,
                                   const net::URLRequestStatus& urs,
                                   const std::string& sec_info) OVERRIDE;

  // N/A to cert downloading.
  virtual void OnDataDownloaded(int request_id, int bytes_downloaded) OVERRIDE;

 private:
  typedef std::vector<std::pair<scoped_refptr<net::IOBuffer>,
                                size_t> > ContentVector;

  void AssembleResource();

  GURL url_;
  net::URLRequest* request_;
  size_t content_length_;
  ContentVector buffer_;
  scoped_refptr<net::IOBuffer> read_buffer_;
  scoped_refptr<net::IOBuffer> resource_buffer_;  // Downloaded certificate.
  // The id of the |RenderProcessHost| which started the download.
  int render_process_host_id_;
  // The id of the |RenderView| which started the download.
  int render_view_id_;
  net::CertificateMimeType cert_type_;
  DISALLOW_COPY_AND_ASSIGN(CertificateResourceHandler);
};

}  // namespace content

#endif  // CONTENT_BROWSER_LOADER_CERTIFICATE_RESOURCE_HANDLER_H_
