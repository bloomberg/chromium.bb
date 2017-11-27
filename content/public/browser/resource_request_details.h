// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_RESOURCE_REQUEST_DETAILS_H_
#define CONTENT_PUBLIC_BROWSER_RESOURCE_REQUEST_DETAILS_H_

#include <string>

#include "content/public/common/resource_type.h"
#include "net/base/host_port_pair.h"
#include "net/cert/cert_status_flags.h"
#include "net/url_request/url_request_status.h"
#include "url/gurl.h"

namespace net {
class URLRequest;
}

namespace content {

// The ResourceRequestDetails object contains additional details about a
// resource request notification. It copies many of the publicly accessible
// member variables of net::URLRequest, but exists on the UI thread.
struct ResourceRequestDetails {
  ResourceRequestDetails(const net::URLRequest* request, bool has_certificate);

  virtual ~ResourceRequestDetails();

  GURL url;
  GURL original_url;
  std::string method;
  std::string referrer;
  bool has_upload;
  int load_flags;
  net::URLRequestStatus status;
  bool has_certificate;
  net::CertStatus ssl_cert_status;
  ResourceType resource_type;
  net::HostPortPair socket_address;
  // HTTP response code. See HttpResponseHeaders::response_code().
  // -1 if there are no response headers yet.
  int http_response_code;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_RESOURCE_REQUEST_DETAILS_H_
