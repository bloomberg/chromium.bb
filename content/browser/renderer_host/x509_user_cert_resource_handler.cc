// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/x509_user_cert_resource_handler.h"

#include "base/string_util.h"
#include "content/browser/download/download_types.h"
#include "content/browser/renderer_host/resource_dispatcher_host.h"
#include "content/browser/renderer_host/resource_dispatcher_host_request_info.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/resource_response.h"
#include "net/base/io_buffer.h"
#include "net/base/mime_sniffer.h"
#include "net/base/mime_util.h"
#include "net/base/x509_certificate.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_status.h"

X509UserCertResourceHandler::X509UserCertResourceHandler(
    ResourceDispatcherHost* host, net::URLRequest* request,
    int render_process_host_id, int render_view_id)
    : host_(host),
      request_(request),
      content_length_(0),
      read_buffer_(NULL),
      resource_buffer_(NULL),
      render_process_host_id_(render_process_host_id),
      render_view_id_(render_view_id) {
}

bool X509UserCertResourceHandler::OnUploadProgress(int request_id,
                                                   uint64 position,
                                                   uint64 size) {
  return true;
}

bool X509UserCertResourceHandler::OnRequestRedirected(
    int request_id,
    const GURL& url,
    content::ResourceResponse* resp,
    bool* defer) {
  url_ = url;
  return true;
}

bool X509UserCertResourceHandler::OnResponseStarted(
    int request_id,
    content::ResourceResponse* resp) {
  return (resp->mime_type == "application/x-x509-user-cert");
}

bool X509UserCertResourceHandler::OnWillStart(int request_id,
                                              const GURL& url,
                                              bool* defer) {
  return true;
}

bool X509UserCertResourceHandler::OnWillRead(int request_id,
                                             net::IOBuffer** buf,
                                             int* buf_size,
                                             int min_size) {
  // TODO(gauravsh): Should we use 'min_size' here?
  DCHECK(buf && buf_size);
  if (!read_buffer_) {
    read_buffer_ = new net::IOBuffer(kReadBufSize);
  }
  *buf = read_buffer_.get();
  *buf_size = kReadBufSize;

  return true;
}

bool X509UserCertResourceHandler::OnReadCompleted(int request_id,
                                                  int* bytes_read) {
  if (!*bytes_read)
    return true;

  // We have more data to read.
  DCHECK(read_buffer_);
  content_length_ += *bytes_read;

  // Release the ownership of the buffer, and store a reference
  // to it. A new one will be allocated in OnWillRead().
  net::IOBuffer* buffer = NULL;
  read_buffer_.swap(&buffer);
  // TODO(gauravsh): Should this be handled by a separate thread?
  buffer_.push_back(std::make_pair(buffer, *bytes_read));

  return true;
}

bool X509UserCertResourceHandler::OnResponseCompleted(
    int request_id,
    const net::URLRequestStatus& urs,
    const std::string& sec_info) {
  if (urs.status() != net::URLRequestStatus::SUCCESS)
    return false;

  // TODO(gauravsh): Verify that 'request_id' was actually a keygen form post
  // and only then import the certificate.
  AssembleResource();
  scoped_refptr<net::X509Certificate> cert(
      net::X509Certificate::CreateFromBytes(resource_buffer_->data(),
                                            content_length_));
  content::GetContentClient()->browser()->AddNewCertificate(
      request_, cert, render_process_host_id_, render_view_id_);
  return true;
}

void X509UserCertResourceHandler::OnRequestClosed() {
}

X509UserCertResourceHandler::~X509UserCertResourceHandler() {
}

void X509UserCertResourceHandler::AssembleResource() {
  size_t assembled_bytes = 0;
  resource_buffer_ = content::AssembleData(buffer_, &assembled_bytes);
  DCHECK_EQ(content_length_, assembled_bytes);
}
