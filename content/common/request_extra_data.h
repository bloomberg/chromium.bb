// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_REQUEST_EXTRA_DATA_H_
#define CONTENT_COMMON_REQUEST_EXTRA_DATA_H_
#pragma once

#include "content/common/page_transition_types.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebURLRequest.h"

// The RenderView stores an instance of this class in the "extra data" of each
// ResourceRequest (see RenderView::willSendRequest).
class RequestExtraData : public WebKit::WebURLRequest::ExtraData {
 public:
  RequestExtraData(bool is_main_frame,
                   int64 frame_id,
                   PageTransition::Type transition_type);
  virtual ~RequestExtraData();

  bool is_main_frame() const { return is_main_frame_; }
  int64 frame_id() const { return frame_id_; }
  PageTransition::Type transition_type() const { return transition_type_; }

 private:
  bool is_main_frame_;
  int64 frame_id_;
  PageTransition::Type transition_type_;

  DISALLOW_COPY_AND_ASSIGN(RequestExtraData);
};

#endif  // CONTENT_COMMON_REQUEST_EXTRA_DATA_H_
