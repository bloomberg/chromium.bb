// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/gview_request_interceptor.h"

#include "base/file_path.h"
#include "base/path_service.h"
#include "chrome/browser/chrome_plugin_service_filter.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/browser/resource_request_info.h"
#include "googleurl/src/gurl.h"
#include "net/base/escape.h"
#include "net/base/load_flags.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_redirect_job.h"
#include "webkit/plugins/webplugininfo.h"

using content::PluginService;
using content::ResourceRequestInfo;

namespace chromeos {

namespace {

// The PDF mime type is treated special if the browser has a built-in
// PDF viewer plug-in installed - we want to intercept only if we're
// told to.
const char kPdfMimeType[] = "application/pdf";

// This is the list of mime types currently supported by the Google
// Document Viewer.
const char* const kSupportedMimeTypeList[] = {
  kPdfMimeType,
  "application/vnd.ms-powerpoint"
};

const char kGViewUrlPrefix[] = "http://docs.google.com/gview?url=";

}  // namespace

GViewRequestInterceptor::GViewRequestInterceptor() {
  for (size_t i = 0; i < arraysize(kSupportedMimeTypeList); ++i) {
    supported_mime_types_.insert(kSupportedMimeTypeList[i]);
  }
}

GViewRequestInterceptor::~GViewRequestInterceptor() {
}

net::URLRequestJob* GViewRequestInterceptor::MaybeIntercept(
    net::URLRequest* request, net::NetworkDelegate* network_delegate) const {
  // Don't attempt to intercept here as we want to wait until the mime
  // type is fully determined.
  return NULL;
}

net::URLRequestJob* GViewRequestInterceptor::MaybeInterceptRedirect(
    const GURL& location,
    net::URLRequest* request,
    net::NetworkDelegate* network_delegate) const {
  return NULL;
}

bool GViewRequestInterceptor::ShouldUsePdfPlugin(
    net::URLRequest* request) const {
  FilePath pdf_path;
  PathService::Get(chrome::FILE_PDF_PLUGIN, &pdf_path);
  const ResourceRequestInfo* info = ResourceRequestInfo::ForRequest(request);
  if (!info)
    return false;

  webkit::WebPluginInfo plugin;
  if (!PluginService::GetInstance()->GetPluginInfoByPath(pdf_path, &plugin)) {
    return false;
  }

  return ChromePluginServiceFilter::GetInstance()->ShouldUsePlugin(
      info->GetChildID(), info->GetRouteID(), info->GetContext(),
      request->url(), GURL(), &plugin);
}

bool GViewRequestInterceptor::ShouldInterceptScheme(
    const std::string& scheme) const {
  return scheme == chrome::kHttpScheme;
}

net::URLRequestJob* GViewRequestInterceptor::MaybeInterceptResponse(
    net::URLRequest* request, net::NetworkDelegate* network_delegate) const {
  // Do not intercept this request if it is a download.
  if (request->load_flags() & net::LOAD_IS_DOWNLOAD) {
    return NULL;
  }

  std::string mime_type;
  request->GetMimeType(&mime_type);

  if (request->method() != "GET" ||
      !ShouldInterceptScheme(request->original_url().scheme())) {
    return NULL;
  }

  // If the local PDF viewing plug-in is installed and enabled, don't
  // redirect PDF files to Google Document Viewer.
  if (mime_type == kPdfMimeType && ShouldUsePdfPlugin(request))
    return NULL;

  // If supported, build the URL to the Google Document Viewer
  // including the origial document's URL, then create a new job that
  // will redirect the browser to this new URL.
  if (supported_mime_types_.count(mime_type) > 0) {
    std::string url(kGViewUrlPrefix);
    url += net::EscapePath(request->url().spec());
    return new net::URLRequestRedirectJob(
        request, network_delegate, GURL(url),
        net::URLRequestRedirectJob::REDIRECT_302_FOUND);
  }
  return NULL;
}

}  // namespace chromeos
