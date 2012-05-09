// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/matcher/substring_set_matcher.h"

#include <queue>

#include "base/logging.h"
#include "base/stl_util.h"

namespace extensions {

//
// SubstringPattern
//

SubstringPattern::SubstringPattern(const std::string& pattern,
                                   SubstringPattern::ID id)
    : pattern_(pattern), id_(id) {}

SubstringPattern::~SubstringPattern() {}

bool SubstringPattern::operator<(const SubstringPattern& rhs) const {
  if (id_ < rhs.id_) return true;
  if (id_ > rhs.id_) return false;
  return pattern_ < rhs.pattern_;
}

//
// SubstringSetMatcher
//

SubstringSetMatcher::SubstringSetMatcher() {
  RebuildAhoCorasickTree();
}

SubstringSetMatcher::~SubstringSetMatcher() {}

void SubstringSetMatcher::RegisterPatterns(
    const std::vector<const SubstringPattern*>& patterns) {
  RegisterAndUnregisterPatterns(patterns,
                                std::vector<const SubstringPattern*>());
}

void SubstringSetMatcher::UnregisterPatterns(
    const std::vector<const SubstringPattern*>& patterns) {
  RegisterAndUnregisterPatterns(std::vector<const SubstringPattern*>(),
                                patterns);
}

void SubstringSetMatcher::RegisterAndUnregisterPatterns(
      const std::vector<const SubstringPattern*>& to_register,
      const std::vector<const SubstringPattern*>& to_unregister) {
  // Register patterns.
  for (std::vector<const SubstringPattern*>::const_iterator i =
      to_register.begin(); i != to_register.end(); ++i) {
    DCHECK(patterns_.find((*i)->id()) == patterns_.end());
    patterns_[(*i)->id()] = *i;
  }

  // Unregister patterns
  for (std::vector<const SubstringPattern*>::const_iterator i =
      to_unregister.begin(); i != to_unregister.end(); ++i) {
    patterns_.erase((*i)->id());
  }

  RebuildAhoCorasickTree();
}

bool SubstringSetMatcher::Match(const std::string& text,
                                std::set<SubstringPattern::ID>* matches) const {
  size_t old_number_of_matches = matches->size();

  // Handle patterns matching the empty string.
  matches->insert(tree_[0].matches().begin(), tree_[0].matches().end());

  int current_node = 0;
  size_t text_length = text.length();
  for (size_t i = 0; i < text_length; ++i) {
    while (!tree_[current_node].HasEdge(text[i]) && current_node != 0)
      current_node = tree_[current_node].failure();
    if (tree_[current_node].HasEdge(text[i])) {
      current_node = tree_[current_node].GetEdge(text[i]);
      matches->insert(tree_[current_node].matches().begin(),
                      tree_[current_node].matches().end());
    } else {
      DCHECK_EQ(0, current_node);
    }
  }

  return old_number_of_matches != matches->size();
}

bool SubstringSetMatcher::IsEmpty() const {
  // An empty tree consists of only the root node.
  return patterns_.empty() && tree_.size() == 1u;
}

void SubstringSetMatcher::RebuildAhoCorasickTree() {
  tree_.clear();

  // Initialize root note of tree.
  AhoCorasickNode root;
  root.set_failure(0);
  tree_.push_back(root);

  // Insert all patterns.
  for (SubstringPatternSet::const_iterator i = patterns_.begin();
       i != patterns_.end(); ++i) {
    InsertPatternIntoAhoCorasickTree(i->second);
  }

  CreateFailureEdges();
}

void SubstringSetMatcher::InsertPatternIntoAhoCorasickTree(
    const SubstringPattern* pattern) {
  const std::string& text = pattern->pattern();
  size_t text_length = text.length();

  // Iterators on the tree and the text.
  int current_node = 0;
  size_t text_pos = 0;

  // Follow existing paths for as long as possible.
  while (text_pos < text_length &&
         tree_[current_node].HasEdge(text[text_pos])) {
    current_node = tree_[current_node].GetEdge(text[text_pos]);
    ++text_pos;
  }

  // Create new nodes if necessary.
  while (text_pos < text_length) {
    AhoCorasickNode new_node;
    tree_.push_back(new_node);
    tree_[current_node].SetEdge(text[text_pos], tree_.size() - 1);
    current_node = tree_.size() - 1;
    ++text_pos;
  }

  // Register match.
  tree_[current_node].AddMatch(pattern->id());
}

void SubstringSetMatcher::CreateFailureEdges() {
  typedef AhoCorasickNode::Edges Edges;

  std::queue<int> queue;

  AhoCorasickNode& root = tree_[0];
  root.set_failure(0);
  const AhoCorasickNode::Edges& root_edges = root.edges();
  for (Edges::const_iterator e = root_edges.begin(); e != root_edges.end();
       ++e) {
    int leads_to = e->second;
    tree_[leads_to].set_failure(0);
    queue.push(leads_to);
  }

  while (!queue.empty()) {
    AhoCorasickNode& current_node = tree_[queue.front()];
    queue.pop();
    for (Edges::const_iterator e = current_node.edges().begin();
         e != current_node.edges().end(); ++e) {
      char edge_label = e->first;
      int leads_to = e->second;
      queue.push(leads_to);

      int failure = current_node.failure();
      while (!tree_[failure].HasEdge(edge_label) && failure != 0)
        failure = tree_[failure].failure();

      int follow_in_case_of_failure = tree_[failure].HasEdge(e->first) ?
          tree_[failure].GetEdge(edge_label) : 0;
      tree_[leads_to].set_failure(follow_in_case_of_failure);
      tree_[leads_to].AddMatches(tree_[follow_in_case_of_failure].matches());
    }
  }
}

SubstringSetMatcher::AhoCorasickNode::AhoCorasickNode()
    : failure_(-1) {}

SubstringSetMatcher::AhoCorasickNode::~AhoCorasickNode() {}

SubstringSetMatcher::AhoCorasickNode::AhoCorasickNode(
    const SubstringSetMatcher::AhoCorasickNode& other)
    : edges_(other.edges_),
      failure_(other.failure_),
      matches_(other.matches_) {}

SubstringSetMatcher::AhoCorasickNode&
SubstringSetMatcher::AhoCorasickNode::operator=(
    const SubstringSetMatcher::AhoCorasickNode& other) {
  edges_ = other.edges_;
  failure_ = other.failure_;
  matches_ = other.matches_;
  return *this;
}

bool SubstringSetMatcher::AhoCorasickNode::HasEdge(char c) const {
  return edges_.find(c) != edges_.end();
}

int SubstringSetMatcher::AhoCorasickNode::GetEdge(char c) const {
  std::map<char, int>::const_iterator i = edges_.find(c);
  return i == edges_.end() ? -1 : i->second;
}

void SubstringSetMatcher::AhoCorasickNode::SetEdge(char c, int node) {
  edges_[c] = node;
}

void SubstringSetMatcher::AhoCorasickNode::AddMatch(SubstringPattern::ID id) {
  matches_.insert(id);
}

void SubstringSetMatcher::AhoCorasickNode::AddMatches(
    const SubstringSetMatcher::AhoCorasickNode::Matches& matches) {
  matches_.insert(matches.begin(), matches.end());
}

}  // namespace extensions
