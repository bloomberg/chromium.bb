// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HTML_VIEWER_TEST_HTML_VIEWER_IMPL_H_
#define COMPONENTS_HTML_VIEWER_TEST_HTML_VIEWER_IMPL_H_

#include "base/basictypes.h"
#include "components/html_viewer/public/interfaces/test_html_viewer.mojom.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/binding.h"

namespace blink {
class WebFrame;
}

namespace html_viewer {

// Caller must ensure that |web_frame| outlives TestHTMLViewerImpl.
class TestHTMLViewerImpl : public TestHTMLViewer {
 public:
  TestHTMLViewerImpl(blink::WebFrame* web_frame,
                     mojo::InterfaceRequest<TestHTMLViewer> request);
  ~TestHTMLViewerImpl() override;

 private:
  // TestHTMLViewer:
  void GetContentAsText(const GetContentAsTextCallback& callback) override;

  blink::WebFrame* web_frame_;
  mojo::Binding<TestHTMLViewer> binding_;

  DISALLOW_COPY_AND_ASSIGN(TestHTMLViewerImpl);
};

}  // namespace html_viewer

#endif  // COMPONENTS_HTML_VIEWER_TEST_HTML_VIEWER_IMPL_H_
