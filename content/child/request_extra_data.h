// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_REQUEST_EXTRA_DATA_H_
#define CONTENT_CHILD_REQUEST_EXTRA_DATA_H_

#include "base/compiler_specific.h"
#include "content/common/content_export.h"
#include "content/public/common/page_transition_types.h"
#include "third_party/WebKit/public/web/WebPageVisibilityState.h"
#include "webkit/child/weburlrequest_extradata_impl.h"

namespace content {

// The RenderView stores an instance of this class in the "extra data" of each
// ResourceRequest (see RenderFrameImpl::willSendRequest).
class CONTENT_EXPORT RequestExtraData
    : NON_EXPORTED_BASE(public webkit_glue::WebURLRequestExtraDataImpl) {
 public:
  RequestExtraData(blink::WebPageVisibilityState visibility_state,
                   const blink::WebString& custom_user_agent,
                   bool was_after_preconnect_request,
                   int render_frame_id,
                   bool is_main_frame,
                   int64 frame_id,
                   const GURL& frame_origin,
                   bool parent_is_main_frame,
                   int64 parent_frame_id,
                   bool allow_download,
                   PageTransition transition_type,
                   bool should_replace_current_entry,
                   int transferred_request_child_id,
                   int transferred_request_request_id);
  virtual ~RequestExtraData();

  blink::WebPageVisibilityState visibility_state() const {
    return visibility_state_;
  }
  int render_frame_id() const { return render_frame_id_; }
  bool is_main_frame() const { return is_main_frame_; }
  int64 frame_id() const { return frame_id_; }
  GURL frame_origin() const { return frame_origin_; }
  bool parent_is_main_frame() const { return parent_is_main_frame_; }
  int64 parent_frame_id() const { return parent_frame_id_; }
  bool allow_download() const { return allow_download_; }
  PageTransition transition_type() const { return transition_type_; }
  bool should_replace_current_entry() const {
    return should_replace_current_entry_;
  }
  int transferred_request_child_id() const {
    return transferred_request_child_id_;
  }
  int transferred_request_request_id() const {
    return transferred_request_request_id_;
  }

 private:
  blink::WebPageVisibilityState visibility_state_;
  int render_frame_id_;
  bool is_main_frame_;
  int64 frame_id_;
  GURL frame_origin_;
  bool parent_is_main_frame_;
  int64 parent_frame_id_;
  bool allow_download_;
  PageTransition transition_type_;
  bool should_replace_current_entry_;
  int transferred_request_child_id_;
  int transferred_request_request_id_;

  DISALLOW_COPY_AND_ASSIGN(RequestExtraData);
};

}  // namespace content

#endif  // CONTENT_CHILD_REQUEST_EXTRA_DATA_H_
