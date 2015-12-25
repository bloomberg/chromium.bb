// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/one_shot_accessibility_tree_search.h"

#include <stdint.h>

#include "base/i18n/case_conversion.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "content/browser/accessibility/browser_accessibility.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"

namespace content {

// Given a node, populate a vector with all of the strings from that node's
// attributes that might be relevant for a text search.
void GetNodeStrings(BrowserAccessibility* node,
                    std::vector<base::string16>* strings) {
  if (node->HasStringAttribute(ui::AX_ATTR_NAME))
    strings->push_back(node->GetString16Attribute(ui::AX_ATTR_NAME));
  if (node->HasStringAttribute(ui::AX_ATTR_DESCRIPTION))
    strings->push_back(node->GetString16Attribute(ui::AX_ATTR_DESCRIPTION));
  if (node->HasStringAttribute(ui::AX_ATTR_VALUE))
    strings->push_back(node->GetString16Attribute(ui::AX_ATTR_VALUE));
  if (node->HasStringAttribute(ui::AX_ATTR_PLACEHOLDER))
    strings->push_back(node->GetString16Attribute(ui::AX_ATTR_PLACEHOLDER));
}

OneShotAccessibilityTreeSearch::OneShotAccessibilityTreeSearch(
    BrowserAccessibility* scope)
    : tree_(scope->manager()),
      scope_node_(scope),
      start_node_(scope),
      direction_(OneShotAccessibilityTreeSearch::FORWARDS),
      result_limit_(UNLIMITED_RESULTS),
      immediate_descendants_only_(false),
      visible_only_(false),
      did_search_(false) {
}

OneShotAccessibilityTreeSearch::~OneShotAccessibilityTreeSearch() {
}

void OneShotAccessibilityTreeSearch::SetStartNode(
    BrowserAccessibility* start_node) {
  DCHECK(!did_search_);
  CHECK(start_node);

  if (!scope_node_->GetParent() ||
      start_node->IsDescendantOf(scope_node_->GetParent())) {
    start_node_ = start_node;
  }
}

void OneShotAccessibilityTreeSearch::SetDirection(Direction direction) {
  DCHECK(!did_search_);
  direction_ = direction;
}

void OneShotAccessibilityTreeSearch::SetResultLimit(int result_limit) {
  DCHECK(!did_search_);
  result_limit_ = result_limit;
}

void OneShotAccessibilityTreeSearch::SetImmediateDescendantsOnly(
    bool immediate_descendants_only) {
  DCHECK(!did_search_);
  immediate_descendants_only_ = immediate_descendants_only;
}

void OneShotAccessibilityTreeSearch::SetVisibleOnly(bool visible_only) {
  DCHECK(!did_search_);
  visible_only_ = visible_only;
}

void OneShotAccessibilityTreeSearch::SetSearchText(const std::string& text) {
  DCHECK(!did_search_);
  search_text_ = text;
}

void OneShotAccessibilityTreeSearch::AddPredicate(
    AccessibilityMatchPredicate predicate) {
  DCHECK(!did_search_);
  predicates_.push_back(predicate);
}

size_t OneShotAccessibilityTreeSearch::CountMatches() {
  if (!did_search_)
    Search();

  return matches_.size();
}

BrowserAccessibility* OneShotAccessibilityTreeSearch::GetMatchAtIndex(
    size_t index) {
  if (!did_search_)
    Search();

  CHECK(index < matches_.size());
  return matches_[index];
}

void OneShotAccessibilityTreeSearch::Search()
{
  if (immediate_descendants_only_) {
    SearchByIteratingOverChildren();
  } else {
    SearchByWalkingTree();
  }
  did_search_ = true;
}

void OneShotAccessibilityTreeSearch::SearchByIteratingOverChildren() {
  // Iterate over the children of scope_node_.
  // If start_node_ is specified, iterate over the first child past that
  // node.

  uint32_t count = scope_node_->PlatformChildCount();
  if (count == 0)
    return;

  // We only care about immediate children of scope_node_, so walk up
  // start_node_ until we get to an immediate child. If it isn't a child,
  // we ignore start_node_.
  while (start_node_ && start_node_->GetParent() != scope_node_)
    start_node_ = start_node_->GetParent();

  uint32_t index = (direction_ == FORWARDS ? 0 : count - 1);
  if (start_node_) {
    index = start_node_->GetIndexInParent();
    if (direction_ == FORWARDS)
      index++;
    else
      index--;
  }

  while (index < count &&
         (result_limit_ == UNLIMITED_RESULTS ||
          static_cast<int>(matches_.size()) < result_limit_)) {
    BrowserAccessibility* node = scope_node_->PlatformGetChild(index);
    if (Matches(node))
      matches_.push_back(node);

    if (direction_ == FORWARDS)
      index++;
    else
      index--;
  }
}

void OneShotAccessibilityTreeSearch::SearchByWalkingTree() {
  BrowserAccessibility* node = nullptr;
  node = start_node_;
  if (node != scope_node_) {
    if (direction_ == FORWARDS)
      node = tree_->NextInTreeOrder(start_node_);
    else
      node = tree_->PreviousInTreeOrder(start_node_);
  }

  BrowserAccessibility* stop_node = scope_node_->GetParent();
  while (node &&
         node != stop_node &&
         (result_limit_ == UNLIMITED_RESULTS ||
          static_cast<int>(matches_.size()) < result_limit_)) {
    if (Matches(node))
      matches_.push_back(node);

    if (direction_ == FORWARDS)
      node = tree_->NextInTreeOrder(node);
    else
      node = tree_->PreviousInTreeOrder(node);
  }
}

bool OneShotAccessibilityTreeSearch::Matches(BrowserAccessibility* node) {
  for (size_t i = 0; i < predicates_.size(); ++i) {
    if (!predicates_[i](start_node_, node))
      return false;
  }

  if (visible_only_) {
    if (node->HasState(ui::AX_STATE_INVISIBLE) ||
        node->HasState(ui::AX_STATE_OFFSCREEN)) {
      return false;
    }
  }

  if (!search_text_.empty()) {
    base::string16 search_text_lower =
      base::i18n::ToLower(base::UTF8ToUTF16(search_text_));
    std::vector<base::string16> node_strings;
    GetNodeStrings(node, &node_strings);
    bool found_text_match = false;
    for (size_t i = 0; i < node_strings.size(); ++i) {
      base::string16 node_string_lower =
        base::i18n::ToLower(node_strings[i]);
      if (node_string_lower.find(search_text_lower) !=
          base::string16::npos) {
        found_text_match = true;
        break;
      }
    }
    if (!found_text_match)
      return false;
  }

  return true;
}

}  // namespace content
