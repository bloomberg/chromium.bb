// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_MATCHER_SUBSTRING_SET_MATCHER_H_
#define CHROME_COMMON_EXTENSIONS_MATCHER_SUBSTRING_SET_MATCHER_H_
#pragma once

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/basictypes.h"

namespace extensions {

// An individual pattern of the SubstringSetMatcher. A pattern consists of
// a string (interpreted as individual bytes, no character encoding) and an
// identifier.
// Each pattern that matches a string emits its ID to the caller of
// SubstringSetMatcher::Match(). This helps the caller to figure out what
// patterns matched a string. All patterns registered to a SubstringSetMatcher
// need to contain unique IDs.
class SubstringPattern {
 public:
  typedef int ID;

  SubstringPattern(const std::string& pattern, ID id);
  ~SubstringPattern();
  const std::string& pattern() const { return pattern_; }
  ID id() const { return id_; }

  bool operator<(const SubstringPattern& rhs) const;

 private:
  std::string pattern_;
  ID id_;

  DISALLOW_COPY_AND_ASSIGN(SubstringPattern);
};

// Class that store a set of string patterns and can find for a string S,
// which string patterns occur in S.
class SubstringSetMatcher {
 public:
  SubstringSetMatcher();
  ~SubstringSetMatcher();

  // Registers all |patterns|. The ownership remains with the caller.
  // The same pattern cannot be registered twice and each pattern needs to have
  // a unique ID.
  // Ownership of the patterns remains with the caller.
  void RegisterPatterns(const std::vector<const SubstringPattern*>& patterns);

  // Unregisters the passed |patterns|.
  void UnregisterPatterns(const std::vector<const SubstringPattern*>& patterns);

  // Analogous to RegisterPatterns and UnregisterPatterns but executes both
  // operations in one step, which is cheaper in the execution.
  void RegisterAndUnregisterPatterns(
      const std::vector<const SubstringPattern*>& to_register,
      const std::vector<const SubstringPattern*>& to_unregister);

  // Matches |text| against all registered SubstringPatterns. Stores the IDs
  // of matching patterns in |matches|. |matches| is not cleared before adding
  // to it.
  bool Match(const std::string& text,
             std::set<SubstringPattern::ID>* matches) const;

  // Returns true if this object retains no allocated data. Only for debugging.
  bool IsEmpty() const;

 private:
  // A node of an Aho Corasick Tree. This is implemented according to
  // http://www.cs.uku.fi/~kilpelai/BSA05/lectures/slides04.pdf
  //
  // The algorithm is based on the idea of building a trie of all registered
  // patterns. Each node of the tree is annotated with a set of pattern
  // IDs that are used to report matches.
  //
  // The root of the trie represents an empty match. If we were looking whether
  // any registered pattern matches a text at the beginning of the text (i.e.
  // whether any pattern is a prefix of the text), we could just follow
  // nodes in the trie according to the matching characters in the text.
  // E.g., if text == "foobar", we would follow the trie from the root node
  // to its child labeled 'f', from there to child 'o', etc. In this process we
  // would report all pattern IDs associated with the trie nodes as matches.
  //
  // As we are not looking for all prefix matches but all substring matches,
  // this algorithm would need to compare text.substr(0), text.substr(1), ...
  // against the trie, which is in O(|text|^2).
  //
  // The Aho Corasick algorithm improves this runtime by using failure edges.
  // In case we have found a partial match of length k in the text
  // (text[i, ..., i + k - 1]) in the trie starting at the root and ending at
  // a node at depth k, but cannot find a match in the trie for character
  // text[i + k] at depth k + 1, we follow a failure edge. This edge
  // corresponds to the longest proper suffix of text[i, ..., i + k - 1] that
  // is a prefix of any registered pattern.
  //
  // If your brain thinks "Forget it, let's go shopping.", don't worry.
  // Take a nap and read an introductory text on the Aho Corasick algorithm.
  // It will make sense. Eventually.
  class AhoCorasickNode {
   public:
    // Key: label of the edge, value: node index in |tree_| of parent class.
    typedef std::map<char, int> Edges;
    typedef std::set<SubstringPattern::ID> Matches;

    AhoCorasickNode();
    ~AhoCorasickNode();
    AhoCorasickNode(const AhoCorasickNode& other);
    AhoCorasickNode& operator=(const AhoCorasickNode& other);

    bool HasEdge(char c) const;
    int GetEdge(char c) const;
    void SetEdge(char c, int node);
    const Edges& edges() const { return edges_; }

    int failure() const { return failure_; }
    void set_failure(int failure) { failure_ = failure; }

    void AddMatch(SubstringPattern::ID id);
    void AddMatches(const Matches& matches);
    const Matches& matches() const { return matches_; }

   private:
    // Outgoing edges of current node.
    Edges edges_;

    // Node index that failure edge leads to.
    int failure_;

    // Identifiers of matches.
    Matches matches_;
  };

  void RebuildAhoCorasickTree();

  // Inserts a path for |pattern->pattern()| into the tree and adds
  // |pattern->id()| to the set of matches. Ownership of |pattern| remains with
  // the caller.
  void InsertPatternIntoAhoCorasickTree(const SubstringPattern* pattern);
  void CreateFailureEdges();

  // Set of all registered SubstringPatterns. Used to regenerate the
  // Aho-Corasick tree in case patterns are registered or unregistered.
  typedef std::map<SubstringPattern::ID, const SubstringPattern*>
      SubstringPatternSet;
  SubstringPatternSet patterns_;

  // The nodes of a Aho-Corasick tree.
  std::vector<AhoCorasickNode> tree_;

  DISALLOW_COPY_AND_ASSIGN(SubstringSetMatcher);
};

}  // namespace extensions

#endif  // CHROME_COMMON_EXTENSIONS_MATCHER_SUBSTRING_SET_MATCHER_H_
