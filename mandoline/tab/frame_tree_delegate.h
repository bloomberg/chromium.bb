// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MANDOLINE_TAB_FRAME_TREE_DELEGATE_H_
#define MANDOLINE_TAB_FRAME_TREE_DELEGATE_H_

namespace mandoline {

class Frame;
class MessageEvent;

class FrameTreeDelegate {
 public:
  virtual bool CanPostMessageEventToFrame(const Frame* source,
                                          const Frame* target,
                                          MessageEvent* event) = 0;

 protected:
  virtual ~FrameTreeDelegate() {}
};

}  // namespace mandoline

#endif  // MANDOLINE_TAB_FRAME_TREE_DELEGATE_H_
