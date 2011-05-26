// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_GVIEW_REQUEST_INTERCEPTOR_H__
#define CHROME_BROWSER_CHROMEOS_GVIEW_REQUEST_INTERCEPTOR_H__
#pragma once

#include <string>
#include "base/hash_tables.h"
#include "net/url_request/url_request.h"

template <typename T> struct DefaultSingletonTraits;

namespace chromeos {

// This class integrates the Google Document Viewer into ChromeOS,
// enabling the viewing of supported document types that the user
// clicks on.  This class will intercept requests to supported
// document types (such as PDF) and redirect the request to the Google
// Document Viewer, including the document's original URL as a
// parameter.
class GViewRequestInterceptor : public net::URLRequest::Interceptor {
 public:
  // Always returns NULL because we don't want to attempt a redirect
  // before seeing the detected mime type of the request.
  virtual net::URLRequestJob* MaybeIntercept(net::URLRequest* request);

  // Determines if the requested document can be viewed by the Google
  // Document Viewer.  If it can, returns a net::URLRequestJob that
  // redirects the browser to the view URL.
  virtual net::URLRequestJob* MaybeInterceptResponse(net::URLRequest* request);

  // Singleton accessor.
  static GViewRequestInterceptor* GetInstance();

 private:
  friend struct DefaultSingletonTraits<GViewRequestInterceptor>;

  GViewRequestInterceptor();
  virtual ~GViewRequestInterceptor();

  // The list of supported mime types.
  base::hash_set<std::string> supported_mime_types_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_GVIEW_REQUEST_INTERCEPTOR_H__
