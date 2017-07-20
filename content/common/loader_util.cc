// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/loader_util.h"

#include <string>

#include "content/public/common/resource_response.h"
#include "net/base/mime_sniffer.h"
#include "net/url_request/url_request.h"

namespace content {

bool ShouldSniffContent(net::URLRequest* url_request,
                        ResourceResponse* response) {
  const std::string& mime_type = response->head.mime_type;

  std::string content_type_options;
  url_request->GetResponseHeaderByName("x-content-type-options",
                                       &content_type_options);

  bool sniffing_blocked =
      base::LowerCaseEqualsASCII(content_type_options, "nosniff");
  bool we_would_like_to_sniff =
      net::ShouldSniffMimeType(url_request->url(), mime_type);

  if (!sniffing_blocked && we_would_like_to_sniff) {
    // We're going to look at the data before deciding what the content type
    // is.  That means we need to delay sending the response started IPC.
    VLOG(1) << "To buffer: " << url_request->url().spec();
    return true;
  }

  return false;
}

}  // namespace content
