// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BOOKMARKS_BOOKMARK_INDEX_H_
#define CHROME_BROWSER_BOOKMARKS_BOOKMARK_INDEX_H_

#include <list>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/basictypes.h"

class BookmarkNode;
class Profile;
class QueryNode;
class QueryParser;

namespace bookmark_utils {
struct TitleMatch;
}

namespace history {
class URLDatabase;
}

// BookmarkIndex maintains an index of the titles of bookmarks for quick
// look up. BookmarkIndex is owned and maintained by BookmarkModel, you
// shouldn't need to interact directly with BookmarkIndex.
//
// BookmarkIndex maintains the index (index_) as a map of sets. The map (type
// Index) maps from a lower case string to the set (type NodeSet) of
// BookmarkNodes that contain that string in their title.

class BookmarkIndex {
 public:
  explicit BookmarkIndex(Profile* profile) : profile_(profile) {}

  // Invoked when a bookmark has been added to the model.
  void Add(const BookmarkNode* node);

  // Invoked when a bookmark has been removed from the model.
  void Remove(const BookmarkNode* node);

  // Returns up to |max_count| of bookmarks containing the text |query|.
  void GetBookmarksWithTitlesMatching(
      const std::wstring& query,
      size_t max_count,
      std::vector<bookmark_utils::TitleMatch>* results);

 private:
  typedef std::set<const BookmarkNode*> NodeSet;
  typedef std::map<std::wstring, NodeSet> Index;

  // Used when finding the set of bookmarks that match a query. Each match
  // represents a set of terms (as an interator into the Index) matching the
  // query as well as the set of nodes that contain those terms in their titles.
  struct Match {
    // List of terms matching the query.
    std::list<Index::const_iterator> terms;

    // The set of nodes matching the terms. As an optimization this is empty
    // when we match only one term, and is filled in when we get more than one
    // term. We can do this as when we have only one matching term we know
    // the set of matching nodes is terms.front()->second.
    //
    // Use nodes_begin() and nodes_end() to get an iterator over the set as
    // it handles the necessary switching between nodes and terms.front().
    NodeSet nodes;

    // Returns an iterator to the beginning of the matching nodes. See
    // description of nodes for why this should be used over nodes.begin().
    NodeSet::const_iterator nodes_begin() const;

    // Returns an iterator to the beginning of the matching nodes. See
    // description of nodes for why this should be used over nodes.end().
    NodeSet::const_iterator nodes_end() const;
  };

  typedef std::vector<Match> Matches;

  // Pairs BookmarkNodes and the number of times the nodes' URLs were typed.
  // Used to sort Matches in decreasing order of typed count.
  typedef std::pair<const BookmarkNode*, int> NodeTypedCountPair;
  typedef std::vector<NodeTypedCountPair> NodeTypedCountPairs;

  // Extracts |matches.nodes| into NodeTypedCountPairs and sorts the pairs in
  // decreasing order of typed count.
  void SortMatches(const Matches& matches,
                   NodeTypedCountPairs* node_typed_counts) const;

  // Extracts BookmarkNodes from |match| and retrieves typed counts for each
  // node from the in-memory database. Inserts pairs containing the node and
  // typed count into the vector |node_typed_counts|. |node_typed_counts| is
  // sorted in decreasing order of typed count.
  void ExtractBookmarkNodePairs(history::URLDatabase* url_db,
                                const Match& match,
                                NodeTypedCountPairs* node_typed_counts) const;

  // Sort function for NodeTypedCountPairs. We sort in decreasing order of typed
  // count so that the best matches will always be added to the results.
  static bool NodeTypedCountPairSortFunc(const NodeTypedCountPair& a,
                                         const NodeTypedCountPair& b) {
      return a.second > b.second;
  }

  // Add |node| to |results| if the node matches the query.
  void AddMatchToResults(const BookmarkNode* node,
                         QueryParser* parser,
                         const std::vector<QueryNode*>& query_nodes,
                         std::vector<bookmark_utils::TitleMatch>* results);

  // Populates |matches| for the specified term. If |first_term| is true, this
  // is the first term in the query. Returns true if there is at least one node
  // matching the term.
  bool GetBookmarksWithTitleMatchingTerm(const std::wstring& term,
                                         bool first_term,
                                         Matches* matches);

  // Iterates over |matches| updating each Match's nodes to contain the
  // intersection of the Match's current nodes and the nodes at |index_i|.
  // If the intersection is empty, the Match is removed.
  //
  // This is invoked from GetBookmarksWithTitleMatchingTerm.
  void CombineMatchesInPlace(const Index::const_iterator& index_i,
                             Matches* matches);

  // Iterates over |current_matches| calculating the intersection between the
  // Match's nodes and the nodes at |index_i|. If the intersection between the
  // two is non-empty, a new match is added to |result|.
  //
  // This differs from CombineMatchesInPlace in that if the intersection is
  // non-empty the result is added to result, not combined in place. This
  // variant is used for prefix matching.
  //
  // This is invoked from GetBookmarksWithTitleMatchingTerm.
  void CombineMatches(const Index::const_iterator& index_i,
                      const Matches& current_matches,
                      Matches* result);

  // Returns the set of query words from |query|.
  std::vector<std::wstring> ExtractQueryWords(const std::wstring& query);

  // Adds |node| to |index_|.
  void RegisterNode(const std::wstring& term, const BookmarkNode* node);

  // Removes |node| from |index_|.
  void UnregisterNode(const std::wstring& term, const BookmarkNode* node);

  Index index_;

  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkIndex);
};

#endif  // CHROME_BROWSER_BOOKMARKS_BOOKMARK_INDEX_H_
