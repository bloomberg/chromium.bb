// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_GVIEW_REQUEST_INTERCEPTOR_H__
#define CHROME_BROWSER_CHROMEOS_GVIEW_REQUEST_INTERCEPTOR_H__
#pragma once

#include <string>
#include "base/compiler_specific.h"
#include "base/hash_tables.h"
#include "net/url_request/url_request_job_factory.h"

namespace chromeos {

// This class integrates the Google Document Viewer into ChromeOS,
// enabling the viewing of supported document types that the user
// clicks on.  This class will intercept requests to supported
// document types (such as PDF) and redirect the request to the Google
// Document Viewer, including the document's original URL as a
// parameter.
class GViewRequestInterceptor : public net::URLRequestJobFactory::Interceptor {
 public:
  GViewRequestInterceptor();
  virtual ~GViewRequestInterceptor();

  // Always returns NULL because we don't want to attempt a redirect
  // before seeing the detected mime type of the request.
  virtual net::URLRequestJob* MaybeIntercept(
      net::URLRequest* request) const OVERRIDE;

  // Always returns NULL.
  virtual net::URLRequestJob* MaybeInterceptRedirect(
      const GURL& location,
      net::URLRequest* request) const OVERRIDE;

  // Determines if the requested document can be viewed by the Google
  // Document Viewer.  If it can, returns a net::URLRequestJob that
  // redirects the browser to the view URL.
  virtual net::URLRequestJob* MaybeInterceptResponse(
      net::URLRequest* request) const OVERRIDE;

 private:
  bool ShouldUsePdfPlugin(net::URLRequest* request) const;
  bool ShouldInterceptScheme(const std::string& scheme) const;

  // The list of supported mime types.
  base::hash_set<std::string> supported_mime_types_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_GVIEW_REQUEST_INTERCEPTOR_H__
