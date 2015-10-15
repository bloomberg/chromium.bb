// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEB_VIEW_URL_REQUEST_CLONEABLE_H_
#define COMPONENTS_WEB_VIEW_URL_REQUEST_CLONEABLE_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/time/time.h"
#include "mojo/services/network/public/interfaces/url_loader.mojom.h"
#include "url/gurl.h"

namespace web_view {

// This class caches data associated with a mojo::URLRequest and vends copies
// of the request for the WebView system. Copies are used for repeated
// navigations, but URLRequest structs are not copyable, as they contain
// handles to transfer large amounts of data between processes, so we
// immediately read the data from pipes and keep a copy here.
class URLRequestCloneable {
 public:
  explicit URLRequestCloneable(mojo::URLRequestPtr original_request);
  explicit URLRequestCloneable(const GURL& raw_url);
  ~URLRequestCloneable();

  // Creates a new URLRequest.
  mojo::URLRequestPtr Clone() const;

  base::TimeTicks originating_time() const { return originating_time_; }
  void set_originating_time(base::TimeTicks value) {
    originating_time_ = value;
  }

 private:
  // All of these are straight from mojo::URLRequest.
  mojo::String url_;
  mojo::String method_;
  mojo::Array<mojo::HttpHeaderPtr> headers_;
  uint32_t response_body_buffer_size_;
  bool auto_follow_redirects_;
  bool bypass_cache_;

  // Whether the body mojo array in the |original_request| was null. We keep
  // track of this so we can differentiate between null arrays and empty
  // arrays.
  bool original_body_null_;

  // This is a serialized version of the data from mojo::URLRequest. We copy
  // this data straight out of the data pipes, and then serve it any time that
  // AsURLRequest() is called.
  std::vector<std::string> body_;

  base::TimeTicks originating_time_;

  DISALLOW_COPY_AND_ASSIGN(URLRequestCloneable);
};

}  // namespace web_view

#endif  // COMPONENTS_WEB_VIEW_URL_REQUEST_CLONEABLE_H_
