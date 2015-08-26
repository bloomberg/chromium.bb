// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HTML_VIEWER_HTML_FACTORY_H_
#define COMPONENTS_HTML_VIEWER_HTML_FACTORY_H_

#include "components/html_viewer/html_frame.h"
#include "components/html_viewer/html_widget.h"

namespace html_viewer {

// HTMLFactory is used to create various classes that need to be mocked
// for layout tests.
class HTMLFactory {
 public:
  virtual HTMLFrame* CreateHTMLFrame(HTMLFrame::CreateParams* params) = 0;
  virtual HTMLWidgetRootLocal* CreateHTMLWidgetRootLocal(
      HTMLWidgetRootLocal::CreateParams* params) = 0;

 protected:
  virtual ~HTMLFactory() {}
};

}  // namespace html_viewer

#endif  // COMPONENTS_HTML_VIEWER_HTML_FACTORY_H_
