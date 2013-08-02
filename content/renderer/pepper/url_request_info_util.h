// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PEPPER_URL_REQUEST_INFO_UTIL_H_
#define CONTENT_RENDERER_PEPPER_URL_REQUEST_INFO_UTIL_H_

#include "base/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "content/common/content_export.h"

namespace ppapi {
struct URLRequestInfoData;
}

namespace WebKit {
class WebFrame;
class WebURLRequest;
}

namespace content {

typedef base::Callback<
    void(scoped_ptr<ppapi::URLRequestInfoData> data,
         bool success,
         scoped_ptr<WebKit::WebURLRequest> dest)> CreateWebURLRequestCallback;

// Creates the WebKit URL request from the current request info.  Invokes the
// callback with a bool of true on success, or false if the request is invalid
// (in which case the other callback argument may be partially initialized).
// Any upload files with only resource IDs (no file ref pointers) will be
// populated by this function on success.
CONTENT_EXPORT void CreateWebURLRequest(
    scoped_ptr<ppapi::URLRequestInfoData> data,
    WebKit::WebFrame* frame,
    CreateWebURLRequestCallback callback);

// Returns true if universal access is required to use the given request.
CONTENT_EXPORT bool URLRequestRequiresUniversalAccess(
    const ::ppapi::URLRequestInfoData& data);

}  // namespace content

#endif  // CONTENT_RENDERER_PEPPER_PPB_URL_REQUEST_INFO_UTIL_H_
