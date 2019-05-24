// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/frame_tree_node_id_registry.h"

#include "content/browser/frame_host/frame_tree_node.h"

namespace content {

// static
FrameTreeNodeIdRegistry* FrameTreeNodeIdRegistry::GetInstance() {
  static base::NoDestructor<FrameTreeNodeIdRegistry> instance;
  return instance.get();
}

void FrameTreeNodeIdRegistry::Add(const base::UnguessableToken& id,
                                  const int frame_tree_node_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  bool inserted = (map_.emplace(id, frame_tree_node_id)).second;
  CHECK(inserted);
}

void FrameTreeNodeIdRegistry::Remove(const base::UnguessableToken& id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  map_.erase(id);
}

int FrameTreeNodeIdRegistry::Get(const base::UnguessableToken& id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto iter = map_.find(id);
  if (iter == map_.end())
    return FrameTreeNode::kFrameTreeNodeInvalidId;
  return iter->second;
}

FrameTreeNodeIdRegistry::FrameTreeNodeIdRegistry() = default;

FrameTreeNodeIdRegistry::~FrameTreeNodeIdRegistry() = default;

}  // namespace content
