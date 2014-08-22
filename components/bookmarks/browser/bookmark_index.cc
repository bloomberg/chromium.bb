// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/bookmarks/browser/bookmark_index.h"

#include <algorithm>
#include <functional>
#include <iterator>
#include <list>

#include "base/i18n/case_conversion.h"
#include "base/logging.h"
#include "base/strings/utf_offset_string_conversions.h"
#include "components/bookmarks/browser/bookmark_client.h"
#include "components/bookmarks/browser/bookmark_match.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/query_parser/snippet.h"
#include "third_party/icu/source/common/unicode/normalizer2.h"

namespace bookmarks {

typedef BookmarkClient::NodeTypedCountPair NodeTypedCountPair;
typedef BookmarkClient::NodeTypedCountPairs NodeTypedCountPairs;

namespace {

// Returns a normalized version of the UTF16 string |text|.  If it fails to
// normalize the string, returns |text| itself as a best-effort.
base::string16 Normalize(const base::string16& text) {
  UErrorCode status = U_ZERO_ERROR;
  const icu::Normalizer2* normalizer2 =
      icu::Normalizer2::getInstance(NULL, "nfkc", UNORM2_COMPOSE, status);
  icu::UnicodeString unicode_text(
      text.data(), static_cast<int32_t>(text.length()));
  icu::UnicodeString unicode_normalized_text;
  normalizer2->normalize(unicode_text, unicode_normalized_text, status);
  if (U_FAILURE(status))
    return text;
  return base::string16(unicode_normalized_text.getBuffer(),
                        unicode_normalized_text.length());
}

// Sort functor for NodeTypedCountPairs. We sort in decreasing order of typed
// count so that the best matches will always be added to the results.
struct NodeTypedCountPairSortFunctor
    : std::binary_function<NodeTypedCountPair, NodeTypedCountPair, bool> {
  bool operator()(const NodeTypedCountPair& a,
                  const NodeTypedCountPair& b) const {
    return a.second > b.second;
  }
};

// Extract the const Node* stored in a BookmarkClient::NodeTypedCountPair.
struct NodeTypedCountPairExtractNodeFunctor
    : std::unary_function<NodeTypedCountPair, const BookmarkNode*> {
  const BookmarkNode* operator()(const NodeTypedCountPair& pair) const {
    return pair.first;
  }
};

}  // namespace

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

BookmarkIndex::BookmarkIndex(BookmarkClient* client,
                             const std::string& languages)
    : client_(client),
      languages_(languages) {
  DCHECK(client_);
}

BookmarkIndex::~BookmarkIndex() {
}

void BookmarkIndex::Add(const BookmarkNode* node) {
  if (!node->is_url())
    return;
  std::vector<base::string16> terms =
      ExtractQueryWords(Normalize(node->GetTitle()));
  for (size_t i = 0; i < terms.size(); ++i)
    RegisterNode(terms[i], node);
  terms =
      ExtractQueryWords(CleanUpUrlForMatching(node->url(), languages_, NULL));
  for (size_t i = 0; i < terms.size(); ++i)
    RegisterNode(terms[i], node);
}

void BookmarkIndex::Remove(const BookmarkNode* node) {
  if (!node->is_url())
    return;

  std::vector<base::string16> terms =
      ExtractQueryWords(Normalize(node->GetTitle()));
  for (size_t i = 0; i < terms.size(); ++i)
    UnregisterNode(terms[i], node);
  terms =
      ExtractQueryWords(CleanUpUrlForMatching(node->url(), languages_, NULL));
  for (size_t i = 0; i < terms.size(); ++i)
    UnregisterNode(terms[i], node);
}

void BookmarkIndex::GetBookmarksMatching(const base::string16& input_query,
                                         size_t max_count,
                                         std::vector<BookmarkMatch>* results) {
  const base::string16 query = Normalize(input_query);
  std::vector<base::string16> terms = ExtractQueryWords(query);
  if (terms.empty())
    return;

  Matches matches;
  for (size_t i = 0; i < terms.size(); ++i) {
    if (!GetBookmarksMatchingTerm(terms[i], i == 0, &matches))
      return;
  }

  Nodes sorted_nodes;
  SortMatches(matches, &sorted_nodes);

  // We use a QueryParser to fill in match positions for us. It's not the most
  // efficient way to go about this, but by the time we get here we know what
  // matches and so this shouldn't be performance critical.
  query_parser::QueryParser parser;
  ScopedVector<query_parser::QueryNode> query_nodes;
  parser.ParseQueryNodes(query, &query_nodes.get());

  // The highest typed counts should be at the beginning of the results vector
  // so that the best matches will always be included in the results. The loop
  // that calculates result relevance in HistoryContentsProvider::ConvertResults
  // will run backwards to assure higher relevance will be attributed to the
  // best matches.
  for (Nodes::const_iterator i = sorted_nodes.begin();
       i != sorted_nodes.end() && results->size() < max_count;
       ++i)
    AddMatchToResults(*i, &parser, query_nodes.get(), results);
}

void BookmarkIndex::SortMatches(const Matches& matches,
                                Nodes* sorted_nodes) const {
  NodeSet nodes;
  for (Matches::const_iterator i = matches.begin(); i != matches.end(); ++i) {
#if !defined(OS_ANDROID)
    nodes.insert(i->nodes_begin(), i->nodes_end());
#else
    // Work around a bug in the implementation of std::set::insert in the STL
    // used on android (http://crbug.com/367050).
    for (NodeSet::const_iterator n = i->nodes_begin(); n != i->nodes_end(); ++n)
      nodes.insert(nodes.end(), *n);
#endif
  }
  sorted_nodes->reserve(sorted_nodes->size() + nodes.size());
  if (client_->SupportsTypedCountForNodes()) {
    NodeTypedCountPairs node_typed_counts;
    client_->GetTypedCountForNodes(nodes, &node_typed_counts);
    std::sort(node_typed_counts.begin(),
              node_typed_counts.end(),
              NodeTypedCountPairSortFunctor());
    std::transform(node_typed_counts.begin(),
                   node_typed_counts.end(),
                   std::back_inserter(*sorted_nodes),
                   NodeTypedCountPairExtractNodeFunctor());
  } else {
    sorted_nodes->insert(sorted_nodes->end(), nodes.begin(), nodes.end());
  }
}

void BookmarkIndex::AddMatchToResults(
    const BookmarkNode* node,
    query_parser::QueryParser* parser,
    const query_parser::QueryNodeStarVector& query_nodes,
    std::vector<BookmarkMatch>* results) {
  // Check that the result matches the query.  The previous search
  // was a simple per-word search, while the more complex matching
  // of QueryParser may filter it out.  For example, the query
  // ["thi"] will match the bookmark titled [Thinking], but since
  // ["thi"] is quoted we don't want to do a prefix match.
  query_parser::QueryWordVector title_words, url_words;
  const base::string16 lower_title =
      base::i18n::ToLower(Normalize(node->GetTitle()));
  parser->ExtractQueryWords(lower_title, &title_words);
  base::OffsetAdjuster::Adjustments adjustments;
  parser->ExtractQueryWords(
      CleanUpUrlForMatching(node->url(), languages_, &adjustments),
      &url_words);
  query_parser::Snippet::MatchPositions title_matches, url_matches;
  for (size_t i = 0; i < query_nodes.size(); ++i) {
    const bool has_title_matches =
        query_nodes[i]->HasMatchIn(title_words, &title_matches);
    const bool has_url_matches =
        query_nodes[i]->HasMatchIn(url_words, &url_matches);
    if (!has_title_matches && !has_url_matches)
      return;
    query_parser::QueryParser::SortAndCoalesceMatchPositions(&title_matches);
    query_parser::QueryParser::SortAndCoalesceMatchPositions(&url_matches);
  }
  BookmarkMatch match;
  if (lower_title.length() == node->GetTitle().length()) {
    // Only use title matches if the lowercase string is the same length
    // as the original string, otherwise the matches are meaningless.
    // TODO(mpearson): revise match positions appropriately.
    match.title_match_positions.swap(title_matches);
  }
  // Now that we're done processing this entry, correct the offsets of the
  // matches in |url_matches| so they point to offsets in the original URL
  // spec, not the cleaned-up URL string that we used for matching.
  std::vector<size_t> offsets =
      BookmarkMatch::OffsetsFromMatchPositions(url_matches);
  base::OffsetAdjuster::UnadjustOffsets(adjustments, &offsets);
  url_matches =
      BookmarkMatch::ReplaceOffsetsInMatchPositions(url_matches, offsets);
  match.url_match_positions.swap(url_matches);
  match.node = node;
  results->push_back(match);
}

bool BookmarkIndex::GetBookmarksMatchingTerm(const base::string16& term,
                                                      bool first_term,
                                                      Matches* matches) {
  Index::const_iterator i = index_.lower_bound(term);
  if (i == index_.end())
    return false;

  if (!query_parser::QueryParser::IsWordLongEnoughForPrefixSearch(term)) {
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

std::vector<base::string16> BookmarkIndex::ExtractQueryWords(
    const base::string16& query) {
  std::vector<base::string16> terms;
  if (query.empty())
    return std::vector<base::string16>();
  query_parser::QueryParser parser;
  parser.ParseQueryWords(base::i18n::ToLower(query), &terms);
  return terms;
}

void BookmarkIndex::RegisterNode(const base::string16& term,
                                 const BookmarkNode* node) {
  index_[term].insert(node);
}

void BookmarkIndex::UnregisterNode(const base::string16& term,
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

}  // namespace bookmarks
