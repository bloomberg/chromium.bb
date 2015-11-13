// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HTML_VIEWER_TEST_HTML_VIEWER_IMPL_H_
#define COMPONENTS_HTML_VIEWER_TEST_HTML_VIEWER_IMPL_H_

#include <set>

#include "base/basictypes.h"
#include "components/html_viewer/public/interfaces/test_html_viewer.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace blink {
class WebLocalFrame;
}

namespace html_viewer {

// Caller must ensure that |web_frame| outlives TestHTMLViewerImpl.
class TestHTMLViewerImpl : public TestHTMLViewer {
 public:
  TestHTMLViewerImpl(blink::WebLocalFrame* web_frame,
                     mojo::InterfaceRequest<TestHTMLViewer> request);
  ~TestHTMLViewerImpl() override;

 private:
  class ExecutionCallbackImpl;

  void CallbackCompleted(ExecutionCallbackImpl* callback_impl);

  // TestHTMLViewer:
  void GetContentAsText(const GetContentAsTextCallback& callback) override;
  void ExecuteScript(const mojo::String& script,
                     const ExecuteScriptCallback& callback) override;

  blink::WebLocalFrame* web_frame_;
  mojo::Binding<TestHTMLViewer> binding_;
  std::set<ExecutionCallbackImpl*> callbacks_;

  DISALLOW_COPY_AND_ASSIGN(TestHTMLViewerImpl);
};

}  // namespace html_viewer

#endif  // COMPONENTS_HTML_VIEWER_TEST_HTML_VIEWER_IMPL_H_
