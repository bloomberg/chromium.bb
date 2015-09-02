// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MANDOLINE_TAB_PENDING_WEB_VIEW_LOAD_H_
#define MANDOLINE_TAB_PENDING_WEB_VIEW_LOAD_H_

#include "base/memory/scoped_ptr.h"
#include "mojo/services/network/public/interfaces/url_loader.mojom.h"

namespace mandoline {
class FrameConnection;
}

namespace web_view {

class WebViewImpl;

// PendingWebViewLoad holds the state necessary to service a load of the main
// frame. Once the necessary state has been obtained the load is started.
class PendingWebViewLoad {
 public:
  explicit PendingWebViewLoad(WebViewImpl* web_view);
  ~PendingWebViewLoad();

  void Init(mojo::URLRequestPtr request);

  scoped_ptr<mandoline::FrameConnection> frame_connection() {
    return frame_connection_.Pass();
  }

  bool is_content_handler_id_valid() const {
    return is_content_handler_id_valid_;
  }

 private:
  void OnGotContentHandlerID();

  WebViewImpl* web_view_;

  bool is_content_handler_id_valid_;

  scoped_ptr<mandoline::FrameConnection> frame_connection_;

  DISALLOW_COPY_AND_ASSIGN(PendingWebViewLoad);
};

}  // namespace web_view

#endif  // MANDOLINE_TAB_PENDING_WEB_VIEW_LOAD_H_
