// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/html_viewer/frame_tree_manager.h"

namespace html_viewer {

FrameTreeManager::FrameTreeManager() {
}

FrameTreeManager::~FrameTreeManager() {
}

void FrameTreeManager::OnConnect(mandoline::FrameTreeServerPtr server,
                                 mojo::Array<mandoline::FrameDataPtr> frames) {
}

void FrameTreeManager::OnFrameAdded(mandoline::FrameDataPtr frame) {
}

void FrameTreeManager::OnFrameRemoved(uint32_t frame_id) {
}

}  // namespace mojo
