// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MANDOLINE_TAB_FRAME_TREE_DELEGATE_H_
#define MANDOLINE_TAB_FRAME_TREE_DELEGATE_H_

#include "mandoline/tab/public/interfaces/frame_tree.mojom.h"
#include "mojo/services/network/public/interfaces/url_loader.mojom.h"

namespace mandoline {

class Frame;
class MessageEvent;

class FrameTreeDelegate {
 public:
  virtual bool CanPostMessageEventToFrame(const Frame* source,
                                          const Frame* target,
                                          MessageEvent* event) = 0;
  virtual void LoadingStateChanged(bool loading) = 0;
  virtual void ProgressChanged(double progress) = 0;

  // |source| is requesting a navigation. If |target_type| is
  // |EXISTING_FRAME| then |target_frame| identifies the frame to perform the
  // navigation in, otherwise |target_frame| is not used. |target_frame| may
  // be null, even for |EXISTING_FRAME|.
  virtual void RequestNavigate(Frame* source,
                               NavigationTargetType target_type,
                               Frame* target_frame,
                               mojo::URLRequestPtr request) = 0;

 protected:
  virtual ~FrameTreeDelegate() {}
};

}  // namespace mandoline

#endif  // MANDOLINE_TAB_FRAME_TREE_DELEGATE_H_
