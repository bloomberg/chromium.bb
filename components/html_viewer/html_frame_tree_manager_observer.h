// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HTML_VIEWER_HTML_FRAME_TREE_MANAGER_OBSERVER_H_
#define COMPONENTS_HTML_VIEWER_HTML_FRAME_TREE_MANAGER_OBSERVER_H_

namespace html_viewer {

// Used to inform observers of interesting events to HTMLFrameTreeManager.
class HTMLFrameTreeManagerObserver {
 public:
  // The change_id of the HTMLFrameTreeManager has advanced. In other words,
  // a structure change was done.
  virtual void OnHTMLFrameTreeManagerChangeIdAdvanced() = 0;

  // The HTMLFrameTreeManager was destroyed.
  virtual void OnHTMLFrameTreeManagerDestroyed() = 0;

 protected:
  virtual ~HTMLFrameTreeManagerObserver() {}
};

}  // namespace html_viewer

#endif  // COMPONENTS_HTML_VIEWER_HTML_FRAME_TREE_MANAGER_OBSERVER_H_
