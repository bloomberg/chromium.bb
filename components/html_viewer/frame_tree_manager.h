// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HTML_VIEWER_FRAME_TREE_MANAGER_H_
#define COMPONENTS_HTML_VIEWER_FRAME_TREE_MANAGER_H_

#include "base/basictypes.h"
#include "mandoline/tab/public/interfaces/frame_tree.mojom.h"

namespace html_viewer {

// FrameTreeManager is the FrameTreeClient implementation for an HTMLDocument.
// It keeps the blink frame tree in sync with that of the FrameTreeServer.
// TODO(sky): make it actually do this.
class FrameTreeManager : public mandoline::FrameTreeClient {
 public:
  FrameTreeManager();
  ~FrameTreeManager() override;

 private:
  // mandoline::FrameTreeClient:
  void OnConnect(mandoline::FrameTreeServerPtr server,
                 mojo::Array<mandoline::FrameDataPtr> frames) override;
  void OnFrameAdded(mandoline::FrameDataPtr frame) override;
  void OnFrameRemoved(uint32_t frame_id) override;

  DISALLOW_COPY_AND_ASSIGN(FrameTreeManager);
};

}  // namespace html_viewer

#endif  // COMPONENTS_HTML_VIEWER_FRAME_TREE_MANAGER_H_
