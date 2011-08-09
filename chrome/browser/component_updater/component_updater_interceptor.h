// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPONENT_UPDATER_COMPONENT_UPDATER_INTERCEPTOR_H_
#define CHROME_BROWSER_COMPONENT_UPDATER_COMPONENT_UPDATER_INTERCEPTOR_H_
#pragma once

#include <map>
#include <string>

#include "base/compiler_specific.h"
#include "base/file_path.h"
#include "googleurl/src/gurl.h"
#include "net/url_request/url_request.h"

#if !defined(UNIT_TEST)
#error "use this class only in unit tests"
#endif

// This url request interceptor lets us respond to localhost http request urls
// with the contents of files on disk for use in tests.
class ComponentUpdateInterceptor
    : public net::URLRequest::Interceptor,
      public base::RefCountedThreadSafe<ComponentUpdateInterceptor> {
 public:
  ComponentUpdateInterceptor();

  // When requests for |url| arrive, respond with the contents of |path|. The
  // hostname of |url| must be "localhost" to avoid DNS lookups, and the scheme
  // must be "http".
  void SetResponse(const std::string& url,
                   const std::string& headers,
                   const FilePath& path);

  // Returns how many requests have been issued that have a stored reply.
  int hit_count() const { return hit_count_; }

 private:
  // When computing matches, this ignores the query parameters of the url.
  virtual net::URLRequestJob* MaybeIntercept(net::URLRequest* request) OVERRIDE;

  friend class base::RefCountedThreadSafe<ComponentUpdateInterceptor>;

  virtual ~ComponentUpdateInterceptor();

  struct Response {
    FilePath data_path;
    std::string headers;
  };

  typedef std::map<GURL, Response> ResponseMap;
  ResponseMap responses_;
  int hit_count_;

  DISALLOW_COPY_AND_ASSIGN(ComponentUpdateInterceptor);
};

#endif  // CHROME_BROWSER_COMPONENT_UPDATER_COMPONENT_UPDATER_INTERCEPTOR_H_
