// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HTML_VIEWER_WEB_LAYER_IMPL_H_
#define COMPONENTS_HTML_VIEWER_WEB_LAYER_IMPL_H_

#include <utility>

#include "cc/blink/web_layer_impl.h"

namespace html_viewer {

class HTMLFrame;

class WebLayerImpl : public cc_blink::WebLayerImpl {
 public:
  explicit WebLayerImpl(HTMLFrame* frame);
  ~WebLayerImpl() override;

  // WebLayer implementation.
  void setBounds(const blink::WebSize& bounds) override;
  void setPosition(const blink::WebFloatPoint& position) override;

 private:
  HTMLFrame* frame_;

  DISALLOW_COPY_AND_ASSIGN(WebLayerImpl);
};

}  // namespace html_viewer

#endif  // COMPONENTS_HTML_VIEWER_WEB_LAYER_IMPL_H_
