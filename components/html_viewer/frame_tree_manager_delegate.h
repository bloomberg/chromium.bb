// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HTML_VIEWER_FRAME_TREE_MANAGER_DELEGATE_H_
#define COMPONENTS_HTML_VIEWER_FRAME_TREE_MANAGER_DELEGATE_H_

namespace html_viewer {

class Frame;

class FrameTreeManagerDelegate {
 public:
  // TODO(yzshen): Remove this check once the browser is able to navigate an
  // existing html_viewer instance and about:blank page support is ready.
  virtual bool ShouldNavigateLocallyInMainFrame() = 0;

  virtual void OnFrameDidFinishLoad(Frame* frame) = 0;

 protected:
  virtual ~FrameTreeManagerDelegate() {}
};

}  // namespace html_viewer

#endif  // COMPONENTS_HTML_VIEWER_FRAME_TREE_MANAGER_DELEGATE_H_
