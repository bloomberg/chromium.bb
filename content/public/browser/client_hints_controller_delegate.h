// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_CLIENT_HINTS_CONTROLLER_DELEGATE_H_
#define CONTENT_PUBLIC_BROWSER_CLIENT_HINTS_CONTROLLER_DELEGATE_H_

#include <memory>

#include "base/optional.h"
#include "content/common/content_export.h"
#include "third_party/blink/public/platform/modules/permissions/permission_status.mojom.h"

class GURL;

namespace net {
class HttpRequestHeaders;
}

namespace content {

class CONTENT_EXPORT ClientHintsControllerDelegate {
 public:
  virtual ~ClientHintsControllerDelegate() = default;

  // Returns additional client hints headers that can be attached to the
  // request to |url|.
  virtual void GetAdditionalNavigationRequestClientHintsHeaders(
      const GURL& url,
      net::HttpRequestHeaders* additional_headers) = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_CLIENT_HINTS_CONTROLLER_DELEGATE_H_
