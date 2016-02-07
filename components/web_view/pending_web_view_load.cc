// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/web_view/pending_web_view_load.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "components/web_view/frame_connection.h"
#include "components/web_view/web_view_impl.h"

namespace web_view {

PendingWebViewLoad::PendingWebViewLoad(WebViewImpl* web_view)
    : web_view_(web_view), is_content_handler_id_valid_(false) {}

PendingWebViewLoad::~PendingWebViewLoad() {}

void PendingWebViewLoad::Init(mojo::URLRequestPtr request) {
  DCHECK(!frame_connection_);
  pending_url_ = GURL(request->url.get());
  navigation_start_time_ =
      base::TimeTicks::FromInternalValue(request->originating_time_ticks);
  frame_connection_.reset(new FrameConnection);
  frame_connection_->Init(web_view_->shell_, std::move(request),
                          base::Bind(&PendingWebViewLoad::OnGotContentHandlerID,
                                     base::Unretained(this)));
}

void PendingWebViewLoad::OnGotContentHandlerID() {
  is_content_handler_id_valid_ = true;
  if (web_view_->root_)
    web_view_->OnLoad(pending_url_);
  // The else case is handled by WebViewImpl when it gets the Window (|root_|).
}

}  // namespace web_view
