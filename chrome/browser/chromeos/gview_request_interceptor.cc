// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/gview_request_interceptor.h"

#include "base/file_path.h"
#include "base/path_service.h"
#include "chrome/common/chrome_paths.h"
#include "net/base/escape.h"
#include "net/base/load_flags.h"
#include "net/url_request/url_request_job.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_redirect_job.h"
#include "googleurl/src/gurl.h"
#include "webkit/glue/plugins/plugin_list.h"

namespace chromeos {

// The PDF mime type is treated special if the browser has a built-in
// PDF viewer plug-in installed - we want to intercept only if we're
// told to.
static const char* const kPdfMimeType = "application/pdf";

// This is the list of mime types currently supported by the Google
// Document Viewer.
static const char* const supported_mime_type_list[] = {
  kPdfMimeType,
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
  // If the local PDF viewing plug-in is installed and enabled, don't
  // redirect PDF files to Google Document Viewer.
  if (mime_type == kPdfMimeType) {
    FilePath pdf_path;
    WebPluginInfo info;
    PathService::Get(chrome::FILE_PDF_PLUGIN, &pdf_path);
    if (NPAPI::PluginList::Singleton()->GetPluginInfoByPath(pdf_path, &info) &&
        info.enabled)
      return NULL;
  }
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
