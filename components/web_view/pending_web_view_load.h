// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEB_VIEW_PENDING_WEB_VIEW_LOAD_H_
#define COMPONENTS_WEB_VIEW_PENDING_WEB_VIEW_LOAD_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"
#include "mojo/services/network/public/interfaces/url_loader.mojom.h"
#include "url/gurl.h"

namespace web_view {

class FrameConnection;
class WebViewImpl;

// PendingWebViewLoad holds the state necessary to service a load of the main
// frame. Once the necessary state has been obtained the load is started.
class PendingWebViewLoad {
 public:
  explicit PendingWebViewLoad(WebViewImpl* web_view);
  ~PendingWebViewLoad();

  void Init(mojo::URLRequestPtr request);

  scoped_ptr<FrameConnection> frame_connection() {
    return frame_connection_.Pass();
  }

  bool is_content_handler_id_valid() const {
    return is_content_handler_id_valid_;
  }

  const GURL& pending_url() const { return pending_url_; }

  base::TimeTicks navigation_start_time() const {
    return navigation_start_time_;
  }

 private:
  void OnGotContentHandlerID();

  WebViewImpl* web_view_;

  GURL pending_url_;
  bool is_content_handler_id_valid_;

  scoped_ptr<FrameConnection> frame_connection_;

  base::TimeTicks navigation_start_time_;

  DISALLOW_COPY_AND_ASSIGN(PendingWebViewLoad);
};

}  // namespace web_view

#endif  // COMPONENTS_WEB_VIEW_PENDING_WEB_VIEW_LOAD_H_
