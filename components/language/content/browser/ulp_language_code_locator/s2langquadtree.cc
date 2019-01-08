// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "components/language/content/browser/ulp_language_code_locator/s2langquadtree.h"

#include <string>

#include "third_party/s2cellid/src/s2/s2cellid.h"

S2LangQuadTreeNode::S2LangQuadTreeNode() {}
S2LangQuadTreeNode::S2LangQuadTreeNode(const S2LangQuadTreeNode& other) =
    default;
S2LangQuadTreeNode::~S2LangQuadTreeNode() = default;

std::string S2LangQuadTreeNode::Get(const S2CellId& cell) const {
  const S2LangQuadTreeNode* node = this;
  const std::string* language = &(node->language_);
  for (int current_level = 1; current_level <= cell.level(); ++current_level) {
    const S2LangQuadTreeNode* const child =
        node->GetChild(cell.child_position(current_level));
    if (child == nullptr)
      break;
    node = child;
    if (!node->language_.empty())
      language = &(node->language_);
  }
  return *language;
}

const S2LangQuadTreeNode* S2LangQuadTreeNode::GetChild(
    const int child_index) const {
  auto it = children_.find(child_index);
  return it == children_.end() ? nullptr : &it->second;
}

bool S2LangQuadTreeNode::IsLeaf() const {
  return children_.empty();
}

bool S2LangQuadTreeNode::IsNullLeaf() const {
  return IsLeaf() && language_.empty();
}