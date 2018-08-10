// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_MIME_SNIFFING_THROTTLE_H_
#define CONTENT_COMMON_MIME_SNIFFING_THROTTLE_H_

#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"
#include "content/public/common/url_loader_throttle.h"

namespace content {

// Throttle for mime type sniffing. This may intercept the request and
// modify the response's mime type in the response head.
class CONTENT_EXPORT MimeSniffingThrottle : public URLLoaderThrottle {
 public:
  MimeSniffingThrottle();
  ~MimeSniffingThrottle() override;

  // Implements URLLoaderThrottle.
  void WillProcessResponse(const GURL& response_url,
                           network::ResourceResponseHead* response_head,
                           bool* defer) override;

  // Called from MimeSniffingURLLoader once mime type is ready.
  void ResumeWithNewResponseHead(
      const network::ResourceResponseHead& new_response_head);

 private:
  base::WeakPtrFactory<MimeSniffingThrottle> weak_factory_;
};

}  // namespace content

#endif  // CONTENT_COMMON_MIME_SNIFFING_THROTTLE_H_
