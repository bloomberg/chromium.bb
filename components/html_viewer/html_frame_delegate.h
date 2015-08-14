// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HTML_VIEWER_HTML_FRAME_DELEGATE_H_
#define COMPONENTS_HTML_VIEWER_HTML_FRAME_DELEGATE_H_

#include "components/html_viewer/html_frame.h"

namespace mojo {
class ApplicationImpl;
}

namespace html_viewer {

class HTMLFrameDelegate {
 public:
  // TODO(yzshen): Remove this check once the browser is able to navigate an
  // existing html_viewer instance and about:blank page support is ready.
  virtual bool ShouldNavigateLocallyInMainFrame() = 0;

  // Invoked when the Frame the delegate is attached to finishes loading. This
  // is not invoked for any child frames, only the frame returned from
  // HTMLFrameTreeManager::CreateFrameAndAttachToTree().
  virtual void OnFrameDidFinishLoad() = 0;

  // Returns the ApplicationImpl for the frame.
  virtual mojo::ApplicationImpl* GetApp() = 0;

  // Creates a new HTMLFrame. The delegate must return non-null.
  virtual HTMLFrame* CreateHTMLFrame(HTMLFrame::CreateParams* params) = 0;

 protected:
  virtual ~HTMLFrameDelegate() {}
};

}  // namespace html_viewer

#endif  // COMPONENTS_HTML_VIEWER_HTML_FRAME_DELEGATE_H_
