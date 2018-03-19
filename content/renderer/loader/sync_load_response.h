// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_CHILD_SYNC_LOAD_RESPONSE_H_
#define CONTENT_RENDERER_CHILD_SYNC_LOAD_RESPONSE_H_

#include <string>

#include "base/optional.h"
#include "content/common/content_export.h"
#include "services/network/public/cpp/resource_response_info.h"
#include "services/network/public/mojom/cors.mojom.h"
#include "url/gurl.h"

namespace content {

// See the SyncLoad method. (The name of this struct is not
// suffixed with "Info" because it also contains the response data.)
struct CONTENT_EXPORT SyncLoadResponse : network::ResourceResponseInfo {
  SyncLoadResponse();
  ~SyncLoadResponse();

  // The response error code.
  int error_code = 0;

  // The response extended error code.
  int extended_error_code = 0;

  // Optional CORS error details.
  base::Optional<network::mojom::CORSError> cors_error;

  // The final URL of the response.  This may differ from the request URL in
  // the case of a server redirect.
  GURL url;

  // The response data.
  std::string data;
};

}  // namespace content

#endif  // CONTENT_RENDERER_CHILD_SYNC_LOAD_RESPONSE_H_
