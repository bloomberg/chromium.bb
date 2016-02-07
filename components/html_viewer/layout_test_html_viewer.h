// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HTML_VIEWER_LAYOUT_TEST_HTML_VIEWER_H_
#define COMPONENTS_HTML_VIEWER_LAYOUT_TEST_HTML_VIEWER_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "components/html_viewer/html_viewer.h"
#include "components/html_viewer/web_test_delegate_impl.h"

namespace test_runner {
class WebTestInterfaces;
}

namespace html_viewer {

class LayoutTestHTMLViewer : public HTMLViewer {
 public:
  LayoutTestHTMLViewer();
  ~LayoutTestHTMLViewer() override;

 private:
  void TestFinished();

  // Overridden from ApplicationDelegate:
  void Initialize(mojo::Shell* shell, const std::string& url,
                  uint32_t id) override;

  // Overridden from InterfaceFactory<shell::mojom::ContentHandler>
  void Create(mojo::ApplicationConnection* connection,
              mojo::InterfaceRequest<mojo::shell::mojom::ContentHandler>
                  request) override;

  scoped_ptr<test_runner::WebTestInterfaces> test_interfaces_;
  WebTestDelegateImpl test_delegate_;

  DISALLOW_COPY_AND_ASSIGN(LayoutTestHTMLViewer);
};

}  // namespace html_viewer

#endif  // COMPONENTS_HTML_VIEWER_LAYOUT_TEST_HTML_VIEWER_H_
