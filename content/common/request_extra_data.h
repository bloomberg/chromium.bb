// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_REQUEST_EXTRA_DATA_H_
#define CONTENT_COMMON_REQUEST_EXTRA_DATA_H_
#pragma once

#include "base/compiler_specific.h"
#include "content/common/content_export.h"
#include "content/public/common/page_transition_types.h"
#include "webkit/glue/weburlrequest_extradata_impl.h"

// The RenderView stores an instance of this class in the "extra data" of each
// ResourceRequest (see RenderView::willSendRequest).
class CONTENT_EXPORT RequestExtraData
    : NON_EXPORTED_BASE(public webkit_glue::WebURLRequestExtraDataImpl) {
 public:
  RequestExtraData(WebKit::WebReferrerPolicy referrer_policy,
                   bool is_main_frame,
                   int64 frame_id,
                   bool parent_is_main_frame,
                   int64 parent_frame_id,
                   content::PageTransition transition_type,
                   int transferred_request_child_id,
                   int transferred_request_request_id);
  virtual ~RequestExtraData();

  bool is_main_frame() const { return is_main_frame_; }
  int64 frame_id() const { return frame_id_; }
  bool parent_is_main_frame() const { return parent_is_main_frame_; }
  int64 parent_frame_id() const { return parent_frame_id_; }
  content::PageTransition transition_type() const { return transition_type_; }
  int transferred_request_child_id() const {
    return transferred_request_child_id_;
  }
  int transferred_request_request_id() const {
    return transferred_request_request_id_;
  }

 private:
  bool is_main_frame_;
  int64 frame_id_;
  bool parent_is_main_frame_;
  int64 parent_frame_id_;
  content::PageTransition transition_type_;
  int transferred_request_child_id_;
  int transferred_request_request_id_;

  DISALLOW_COPY_AND_ASSIGN(RequestExtraData);
};

#endif  // CONTENT_COMMON_REQUEST_EXTRA_DATA_H_
