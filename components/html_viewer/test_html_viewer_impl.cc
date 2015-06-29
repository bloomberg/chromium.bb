// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/html_viewer/test_html_viewer_impl.h"

#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebFrame.h"

using blink::WebDocument;
using blink::WebFrame;

namespace html_viewer {

TestHTMLViewerImpl::TestHTMLViewerImpl(
    blink::WebFrame* web_frame,
    mojo::InterfaceRequest<TestHTMLViewer> request)
    : web_frame_(web_frame), binding_(this, request.Pass()) {
}

TestHTMLViewerImpl::~TestHTMLViewerImpl() {
}

void TestHTMLViewerImpl::GetContentAsText(
    const GetContentAsTextCallback& callback) {
  callback.Run(web_frame_->document().contentAsTextForTesting().utf8());
}

}  // namespace html_viewer
