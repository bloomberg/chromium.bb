// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/gview_request_interceptor.h"

#include "net/base/escape.h"
#include "net/base/load_flags.h"
#include "net/url_request/url_request_job.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_redirect_job.h"
#include "googleurl/src/gurl.h"

namespace chromeos {

// This is the list of mime types currently supported by the Google
// Document Viewer.
static const char* const supported_mime_type_list[] = {
  "application/pdf",
  "application/vnd.ms-powerpoint"
};

static const char* const kGViewUrlPrefix = "http://docs.google.com/gview?url=";

GViewRequestInterceptor::GViewRequestInterceptor() {
  URLRequest::RegisterRequestInterceptor(this);
  for (size_t i = 0; i < arraysize(supported_mime_type_list); ++i) {
    supported_mime_types_.insert(supported_mime_type_list[i]);
  }
}

GViewRequestInterceptor::~GViewRequestInterceptor() {
  URLRequest::UnregisterRequestInterceptor(this);
}

URLRequestJob* GViewRequestInterceptor::MaybeIntercept(URLRequest* request) {
  // Don't attempt to intercept here as we want to wait until the mime
  // type is fully determined.
  return NULL;
}

URLRequestJob* GViewRequestInterceptor::MaybeInterceptResponse(
    URLRequest* request) {
  // Do not intercept this request if it is a download.
  if (request->load_flags() & net::LOAD_IS_DOWNLOAD) {
    return NULL;
  }

  std::string mime_type;
  request->GetMimeType(&mime_type);
  // If supported, build the URL to the Google Document Viewer
  // including the origial document's URL, then create a new job that
  // will redirect the browser to this new URL.
  if (supported_mime_types_.count(mime_type) > 0) {
    std::string url(kGViewUrlPrefix);
    url += EscapePath(request->url().spec());
    return new URLRequestRedirectJob(request, GURL(url));
  }
  return NULL;
}

URLRequest::Interceptor* GViewRequestInterceptor::GetGViewRequestInterceptor() {
  return Singleton<GViewRequestInterceptor>::get();
}

}  // namespace chromeos
