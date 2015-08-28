// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/certificate_resource_handler.h"

#include <limits.h>

#include "components/mime_util/mime_util.h"
#include "content/browser/loader/resource_request_info_impl.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/resource_response.h"
#include "net/base/io_buffer.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_status.h"

namespace content {

CertificateResourceHandler::CertificateResourceHandler(net::URLRequest* request)
    : ResourceHandler(request),
      buffer_(new net::GrowableIOBuffer),
      cert_type_(net::CERTIFICATE_MIME_TYPE_UNKNOWN) {
}

CertificateResourceHandler::~CertificateResourceHandler() {
}

bool CertificateResourceHandler::OnRequestRedirected(
    const net::RedirectInfo& redirect_info,
    ResourceResponse* resp,
    bool* defer) {
  return true;
}

bool CertificateResourceHandler::OnResponseStarted(ResourceResponse* resp,
                                                   bool* defer) {
  cert_type_ =
      mime_util::GetCertificateMimeTypeForMimeType(resp->head.mime_type);
  return cert_type_ != net::CERTIFICATE_MIME_TYPE_UNKNOWN;
}

bool CertificateResourceHandler::OnWillStart(const GURL& url, bool* defer) {
  return true;
}

bool CertificateResourceHandler::OnBeforeNetworkStart(const GURL& url,
                                                      bool* defer) {
  return true;
}

bool CertificateResourceHandler::OnWillRead(scoped_refptr<net::IOBuffer>* buf,
                                            int* buf_size,
                                            int min_size) {
  static const int kInitialBufferSizeInBytes = 32768;
  static const int kMaxCertificateSizeInBytes = 1024 * 1024;

  // TODO(gauravsh): Should we use 'min_size' here?
  DCHECK(buf);
  DCHECK(buf_size);

  if (buffer_->capacity() == 0) {
    buffer_->SetCapacity(kInitialBufferSizeInBytes);
  } else if (buffer_->RemainingCapacity() == 0) {
    int capacity = buffer_->capacity();
    if (capacity >= kMaxCertificateSizeInBytes)
      return false;
    static_assert(kMaxCertificateSizeInBytes < INT_MAX / 2,
                  "The size limit ensures the capacity remains in bounds.");
    capacity *= 2;
    if (capacity > kMaxCertificateSizeInBytes)
      capacity = kMaxCertificateSizeInBytes;
    buffer_->SetCapacity(capacity);
  }

  *buf = buffer_.get();
  *buf_size = buffer_->RemainingCapacity();

  return true;
}

bool CertificateResourceHandler::OnReadCompleted(int bytes_read, bool* defer) {
  DCHECK_LE(0, bytes_read);
  DCHECK_LE(bytes_read, buffer_->RemainingCapacity());
  if (!bytes_read)
    return true;

  buffer_->set_offset(buffer_->offset() + bytes_read);
  return true;
}

void CertificateResourceHandler::OnResponseCompleted(
    const net::URLRequestStatus& urs,
    const std::string& sec_info,
    bool* defer) {
  if (urs.status() != net::URLRequestStatus::SUCCESS)
    return;

  // Note that it's up to the browser to verify that the certificate
  // data is well-formed.
  const ResourceRequestInfo* info = GetRequestInfo();
  GetContentClient()->browser()->AddCertificate(
      cert_type_, buffer_->StartOfBuffer(),
      static_cast<size_t>(buffer_->offset()), info->GetChildID(),
      info->GetRenderFrameID());
}

void CertificateResourceHandler::OnDataDownloaded(int bytes_downloaded) {
  NOTREACHED();
}

}  // namespace content
