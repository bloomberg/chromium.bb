// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bookmarks/bookmark_index.h"

#include <algorithm>
#include <iterator>
#include <list>

#include "base/i18n/case_conversion.h"
#include "base/string16.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/bookmarks/bookmark_utils.h"
#include "chrome/browser/history/history.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/history/query_parser.h"
#include "chrome/browser/history/url_database.h"
#include "chrome/browser/profiles/profile.h"
#include "ui/base/l10n/l10n_util.h"

// Used when finding the set of bookmarks that match a query. Each match
// represents a set of terms (as an interator into the Index) matching the
// query as well as the set of nodes that contain those terms in their titles.
struct BookmarkIndex::Match {
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

BookmarkIndex::NodeSet::const_iterator
    BookmarkIndex::Match::nodes_begin() const {
  return nodes.empty() ? terms.front()->second.begin() : nodes.begin();
}

BookmarkIndex::NodeSet::const_iterator BookmarkIndex::Match::nodes_end() const {
  return nodes.empty() ? terms.front()->second.end() : nodes.end();
}

BookmarkIndex::BookmarkIndex(content::BrowserContext* browser_context)
    : browser_context_(browser_context) {
}

BookmarkIndex::~BookmarkIndex() {
}

void BookmarkIndex::Add(const BookmarkNode* node) {
  if (!node->is_url())
    return;
  std::vector<string16> terms = ExtractQueryWords(node->GetTitle());
  for (size_t i = 0; i < terms.size(); ++i)
    RegisterNode(terms[i], node);
}

void BookmarkIndex::Remove(const BookmarkNode* node) {
  if (!node->is_url())
    return;

  std::vector<string16> terms = ExtractQueryWords(node->GetTitle());
  for (size_t i = 0; i < terms.size(); ++i)
    UnregisterNode(terms[i], node);
}

void BookmarkIndex::GetBookmarksWithTitlesMatching(
    const string16& query,
    size_t max_count,
    std::vector<bookmark_utils::TitleMatch>* results) {
  std::vector<string16> terms = ExtractQueryWords(query);
  if (terms.empty())
    return;

  Matches matches;
  for (size_t i = 0; i < terms.size(); ++i) {
    if (!GetBookmarksWithTitleMatchingTerm(terms[i], i == 0, &matches))
      return;
  }

  NodeTypedCountPairs node_typed_counts;
  SortMatches(matches, &node_typed_counts);

  // We use a QueryParser to fill in match positions for us. It's not the most
  // efficient way to go about this, but by the time we get here we know what
  // matches and so this shouldn't be performance critical.
  QueryParser parser;
  ScopedVector<QueryNode> query_nodes;
  parser.ParseQueryNodes(query, &query_nodes.get());

  // The highest typed counts should be at the beginning of the results vector
  // so that the best matches will always be included in the results. The loop
  // that calculates result relevance in HistoryContentsProvider::ConvertResults
  // will run backwards to assure higher relevance will be attributed to the
  // best matches.
  for (NodeTypedCountPairs::const_iterator i = node_typed_counts.begin();
       i != node_typed_counts.end() && results->size() < max_count; ++i)
    AddMatchToResults(i->first, &parser, query_nodes.get(), results);
}

void BookmarkIndex::SortMatches(const Matches& matches,
                                NodeTypedCountPairs* node_typed_counts) const {
  HistoryService* const history_service = browser_context_ ?
      HistoryServiceFactory::GetForProfile(
          Profile::FromBrowserContext(browser_context_),
          Profile::EXPLICIT_ACCESS) : NULL;

  history::URLDatabase* url_db = history_service ?
      history_service->InMemoryDatabase() : NULL;

  for (Matches::const_iterator i = matches.begin(); i != matches.end(); ++i)
    ExtractBookmarkNodePairs(url_db, *i, node_typed_counts);

  std::sort(node_typed_counts->begin(), node_typed_counts->end(),
            &NodeTypedCountPairSortFunc);
  // Eliminate duplicates.
  node_typed_counts->erase(std::unique(node_typed_counts->begin(),
                                       node_typed_counts->end()),
                           node_typed_counts->end());
}

void BookmarkIndex::ExtractBookmarkNodePairs(
    history::URLDatabase* url_db,
    const Match& match,
    NodeTypedCountPairs* node_typed_counts) const {

  for (NodeSet::const_iterator i = match.nodes_begin();
       i != match.nodes_end(); ++i) {
    history::URLRow url;
    if (url_db)
      url_db->GetRowForURL((*i)->url(), &url);
    NodeTypedCountPair pair(*i, url.typed_count());
    node_typed_counts->push_back(pair);
  }
}

void BookmarkIndex::AddMatchToResults(
    const BookmarkNode* node,
    QueryParser* parser,
    const std::vector<QueryNode*>& query_nodes,
    std::vector<bookmark_utils::TitleMatch>* results) {
  bookmark_utils::TitleMatch title_match;
  // Check that the result matches the query.  The previous search
  // was a simple per-word search, while the more complex matching
  // of QueryParser may filter it out.  For example, the query
  // ["thi"] will match the bookmark titled [Thinking], but since
  // ["thi"] is quoted we don't want to do a prefix match.
  if (parser->DoesQueryMatch(node->GetTitle(), query_nodes,
                             &(title_match.match_positions))) {
    title_match.node = node;
    results->push_back(title_match);
  }
}

bool BookmarkIndex::GetBookmarksWithTitleMatchingTerm(const string16& term,
                                                      bool first_term,
                                                      Matches* matches) {
  Index::const_iterator i = index_.lower_bound(term);
  if (i == index_.end())
    return false;

  if (!QueryParser::IsWordLongEnoughForPrefixSearch(term)) {
    // Term is too short for prefix match, compare using exact match.
    if (i->first != term)
      return false;  // No bookmarks with this term.

    if (first_term) {
      Match match;
      match.terms.push_back(i);
      matches->push_back(match);
      return true;
    }
    CombineMatchesInPlace(i, matches);
  } else if (first_term) {
    // This is the first term and we're doing a prefix match. Loop through
    // index adding all entries that start with term to matches.
    while (i != index_.end() &&
           i->first.size() >= term.size() &&
           term.compare(0, term.size(), i->first, 0, term.size()) == 0) {
      Match match;
      match.terms.push_back(i);
      matches->push_back(match);
      ++i;
    }
  } else {
    // Prefix match and not the first term. Loop through index combining
    // current matches in matches with term, placing result in result.
    Matches result;
    while (i != index_.end() &&
           i->first.size() >= term.size() &&
           term.compare(0, term.size(), i->first, 0, term.size()) == 0) {
      CombineMatches(i, *matches, &result);
      ++i;
    }
    matches->swap(result);
  }
  return !matches->empty();
}

void BookmarkIndex::CombineMatchesInPlace(const Index::const_iterator& index_i,
                                          Matches* matches) {
  for (size_t i = 0; i < matches->size(); ) {
    Match* match = &((*matches)[i]);
    NodeSet intersection;
    std::set_intersection(match->nodes_begin(), match->nodes_end(),
                          index_i->second.begin(), index_i->second.end(),
                          std::inserter(intersection, intersection.begin()));
    if (intersection.empty()) {
      matches->erase(matches->begin() + i);
    } else {
      match->terms.push_back(index_i);
      match->nodes.swap(intersection);
      ++i;
    }
  }
}

void BookmarkIndex::CombineMatches(const Index::const_iterator& index_i,
                                   const Matches& current_matches,
                                   Matches* result) {
  for (size_t i = 0; i < current_matches.size(); ++i) {
    const Match& match = current_matches[i];
    NodeSet intersection;
    std::set_intersection(match.nodes_begin(), match.nodes_end(),
                          index_i->second.begin(), index_i->second.end(),
                          std::inserter(intersection, intersection.begin()));
    if (!intersection.empty()) {
      result->push_back(Match());
      Match& combined_match = result->back();
      combined_match.terms = match.terms;
      combined_match.terms.push_back(index_i);
      combined_match.nodes.swap(intersection);
    }
  }
}

std::vector<string16> BookmarkIndex::ExtractQueryWords(const string16& query) {
  std::vector<string16> terms;
  if (query.empty())
    return std::vector<string16>();
  QueryParser parser;
  // TODO(brettw): use ICU normalization:
  // http://userguide.icu-project.org/transforms/normalization
  parser.ParseQueryWords(base::i18n::ToLower(query), &terms);
  return terms;
}

void BookmarkIndex::RegisterNode(const string16& term,
                                 const BookmarkNode* node) {
  if (std::find(index_[term].begin(), index_[term].end(), node) !=
      index_[term].end()) {
    // We've already added node for term.
    return;
  }
  index_[term].insert(node);
}

void BookmarkIndex::UnregisterNode(const string16& term,
                                   const BookmarkNode* node) {
  Index::iterator i = index_.find(term);
  if (i == index_.end()) {
    // We can get here if the node has the same term more than once. For
    // example, a bookmark with the title 'foo foo' would end up here.
    return;
  }
  i->second.erase(node);
  if (i->second.empty())
    index_.erase(i);
}
