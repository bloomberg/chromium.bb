// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/web_view/frame_tree_delegate.h"

namespace web_view {

void FrameTreeDelegate::DidCreateFrame(Frame* frame) {}
void FrameTreeDelegate::DidDestroyFrame(Frame* frame) {}
void FrameTreeDelegate::OnWindowEmbeddedInFrameDisconnected(Frame* frame) {}
void FrameTreeDelegate::OnFindInFrameCountUpdated(int32_t request_id,
                                                  Frame* frame,
                                                  int32_t count,
                                                  bool final_update) {}
void FrameTreeDelegate::OnFindInPageSelectionUpdated(
    int32_t request_id,
    Frame* frame,
    int32_t active_match_ordinal) {}

}  // namespace web_view
