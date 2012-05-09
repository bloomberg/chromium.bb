// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_MATCHER_URL_MATCHER_H_
#define CHROME_COMMON_EXTENSIONS_MATCHER_URL_MATCHER_H_
#pragma once

#include <set>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "chrome/common/extensions/matcher/substring_set_matcher.h"

class GURL;

namespace base {
class DictionaryValue;
}

namespace extensions {

// This class represents a single URL matching condition, e.g. a match on the
// host suffix or the containment of a string in the query component of a GURL.
//
// The difference from a simple SubstringPattern is that this also supports
// checking whether the {Host, Path, Query} of a URL contains a string. The
// reduction of URL matching conditions to StringPatterns conducted by
// URLMatcherConditionFactory is not capable of expressing that alone.
class URLMatcherCondition {
 public:
  enum Criterion {
    HOST_PREFIX,
    HOST_SUFFIX,
    HOST_CONTAINS,
    HOST_EQUALS,
    PATH_PREFIX,
    PATH_SUFFIX,
    PATH_CONTAINS,
    PATH_EQUALS,
    QUERY_PREFIX,
    QUERY_SUFFIX,
    QUERY_CONTAINS,
    QUERY_EQUALS,
    HOST_SUFFIX_PATH_PREFIX,
    URL_PREFIX,
    URL_SUFFIX,
    URL_CONTAINS,
    URL_EQUALS,
  };

  URLMatcherCondition();
  ~URLMatcherCondition();
  URLMatcherCondition(Criterion criterion,
                      const SubstringPattern* substring_pattern);
  URLMatcherCondition(const URLMatcherCondition& rhs);
  URLMatcherCondition& operator=(const URLMatcherCondition& rhs);
  bool operator<(const URLMatcherCondition& rhs) const;

  Criterion criterion() const { return criterion_; }
  const SubstringPattern* substring_pattern() const {
    return substring_pattern_;
  }

  // Returns whether this URLMatcherCondition needs to be executed on a
  // full URL rather than the individual components (see
  // URLMatcherConditionFactory).
  bool IsFullURLCondition() const;

  // Returns whether this condition is fulfilled according to
  // |matching_substring_patterns| and |url|.
  bool IsMatch(
      const std::set<SubstringPattern::ID>& matching_substring_patterns,
      const GURL& url) const;

 private:
  // |criterion_| and |substring_pattern_| describe together what property a URL
  // needs to fulfill to be considered a match.
  Criterion criterion_;

  // This is the SubstringPattern that is used in a SubstringSetMatcher.
  const SubstringPattern* substring_pattern_;
};

// Class to map the problem of finding {host, path, query} {prefixes, suffixes,
// containments, and equality} in GURLs to the substring matching problem.
//
// Say, you want to check whether the path of a URL starts with "/index.html".
// This class preprocesses a URL like "www.google.com/index.html" into something
// like "www.google.com|/index.html". After preprocessing, you can search for
// "|/index.html" in the string and see that this candidate URL actually has
// a path that starts with "/index.html". On the contrary,
// "www.google.com/images/index.html" would be normalized to
// "www.google.com|/images/index.html". It is easy to see that it contains
// "/index.html" but the path of the URL does not start with "/index.html".
//
// This preprocessing is important if you want to match a URL against many
// patterns because it reduces the matching to a "discover all substrings
// of a dictionary in a text" problem, which can be solved very efficiently
// by the Aho-Corasick algorithm.
//
// IMPORTANT: The URLMatcherConditionFactory owns the SubstringPattern
// referenced by created URLMatcherConditions. Therefore, it must outlive
// all created URLMatcherCondition and the SubstringSetMatcher.
class URLMatcherConditionFactory {
 public:
  URLMatcherConditionFactory();
  ~URLMatcherConditionFactory();

  // Canonicalizes a URL for "Create{Host,Path,Query}*Condition" searches.
  std::string CanonicalizeURLForComponentSearches(const GURL& url);

  // Factory methods for various condition types.
  URLMatcherCondition CreateHostPrefixCondition(const std::string& prefix);
  URLMatcherCondition CreateHostSuffixCondition(const std::string& suffix);
  URLMatcherCondition CreateHostContainsCondition(const std::string& str);
  URLMatcherCondition CreateHostEqualsCondition(const std::string& str);

  URLMatcherCondition CreatePathPrefixCondition(const std::string& prefix);
  URLMatcherCondition CreatePathSuffixCondition(const std::string& suffix);
  URLMatcherCondition CreatePathContainsCondition(const std::string& str);
  URLMatcherCondition CreatePathEqualsCondition(const std::string& str);

  URLMatcherCondition CreateQueryPrefixCondition(const std::string& prefix);
  URLMatcherCondition CreateQuerySuffixCondition(const std::string& suffix);
  URLMatcherCondition CreateQueryContainsCondition(const std::string& str);
  URLMatcherCondition CreateQueryEqualsCondition(const std::string& str);

  // This covers the common case, where you don't care whether a domain
  // "foobar.com" is expressed as "foobar.com" or "www.foobar.com", and it
  // should be followed by a given |path_prefix|.
  URLMatcherCondition CreateHostSuffixPathPrefixCondition(
      const std::string& host_suffix,
      const std::string& path_prefix);

  // Canonicalizes a URL for "CreateURL*Condition" searches.
  std::string CanonicalizeURLForFullSearches(const GURL& url);

  URLMatcherCondition CreateURLPrefixCondition(const std::string& prefix);
  URLMatcherCondition CreateURLSuffixCondition(const std::string& suffix);
  URLMatcherCondition CreateURLContainsCondition(const std::string& str);
  URLMatcherCondition CreateURLEqualsCondition(const std::string& str);

  // Removes all patterns from |pattern_singletons_| that are not listed in
  // |used_patterns|. These patterns are not referenced any more and get
  // freed.
  void ForgetUnusedPatterns(
      const std::set<SubstringPattern::ID>& used_patterns);

  // Returns true if this object retains no allocated data. Only for debugging.
  bool IsEmpty() const;

 private:
  // Creates a URLMatcherCondition according to the parameters passed.
  // The URLMatcherCondition will refer to a SubstringPattern that is
  // owned by |pattern_singletons_|.
  URLMatcherCondition CreateCondition(URLMatcherCondition::Criterion criterion,
                                      const std::string& pattern);

  // Prepends a "." to the hostname if it does not start with one.
  std::string CanonicalizeHostname(const std::string& hostname) const;

  // Counter that ensures that all created SubstringPatterns have unique IDs.
  int id_counter_;

  // This comparison considers only the pattern() value of the
  // SubstringPatterns.
  struct SubstringPatternPointerCompare {
    bool operator()(SubstringPattern* lhs, SubstringPattern* rhs) const;
  };
  // Set to ensure that we generate only one SubstringPattern for each content
  // of SubstringPattern::pattern().
  typedef std::set<SubstringPattern*, SubstringPatternPointerCompare>
      PatternSingletons;
  PatternSingletons pattern_singletons_;

  DISALLOW_COPY_AND_ASSIGN(URLMatcherConditionFactory);
};

// This class represents a filter for the URL scheme to be hooked up into a
// URLMatcherConditionSet.
class URLMatcherSchemeFilter {
 public:
  explicit URLMatcherSchemeFilter(const std::string& filter);
  explicit URLMatcherSchemeFilter(const std::vector<std::string>& filters);
  ~URLMatcherSchemeFilter();
  bool IsMatch(const GURL& url) const;

 private:
  std::vector<std::string> filters_;

  DISALLOW_COPY_AND_ASSIGN(URLMatcherSchemeFilter);
};

// This class represents a filter for port numbers to be hooked up into a
// URLMatcherConditionSet.
class URLMatcherPortFilter {
 public:
  // Boundaries of a port range (both ends are included).
  typedef std::pair<int, int> Range;
  explicit URLMatcherPortFilter(const std::vector<Range>& ranges);
  ~URLMatcherPortFilter();
  bool IsMatch(const GURL& url) const;

  // Creates a port range [from, to]; both ends are included.
  static Range CreateRange(int from, int to);
  // Creates a port range containing a single port.
  static Range CreateRange(int port);

 private:
  std::vector<Range> ranges_;

  DISALLOW_COPY_AND_ASSIGN(URLMatcherPortFilter);
};

// This class represents a set of conditions that all need to match on a
// given URL in order to be considered a match.
class URLMatcherConditionSet : public base::RefCounted<URLMatcherConditionSet> {
 public:
  typedef int ID;
  typedef std::set<URLMatcherCondition> Conditions;
  typedef std::vector<scoped_refptr<URLMatcherConditionSet> > Vector;

  // Matches if all conditions in |conditions| are fulfilled.
  URLMatcherConditionSet(ID id, const Conditions& conditions);

  // Matches if all conditions in |conditions|, |scheme_filter| and
  // |port_filter| are fulfilled. |scheme_filter| and |port_filter| may be NULL,
  // in which case, no restrictions are imposed on the scheme/port of a URL.
  URLMatcherConditionSet(ID id, const Conditions& conditions,
                         scoped_ptr<URLMatcherSchemeFilter> scheme_filter,
                         scoped_ptr<URLMatcherPortFilter> port_filter);

  ID id() const { return id_; }
  const Conditions& conditions() const { return conditions_; }

  bool IsMatch(
      const std::set<SubstringPattern::ID>& matching_substring_patterns,
      const GURL& url) const;

 private:
  friend class base::RefCounted<URLMatcherConditionSet>;
  ~URLMatcherConditionSet();
  ID id_;
  Conditions conditions_;
  scoped_ptr<URLMatcherSchemeFilter> scheme_filter_;
  scoped_ptr<URLMatcherPortFilter> port_filter_;

  DISALLOW_COPY_AND_ASSIGN(URLMatcherConditionSet);
};

// This class allows matching one URL against a large set of
// URLMatcherConditionSets at the same time.
class URLMatcher {
 public:
  URLMatcher();
  ~URLMatcher();

  // Adds new URLMatcherConditionSet to this URL Matcher. Each condition set
  // must have a unique ID.
  // This is an expensive operation as it triggers pre-calculations on the
  // currently registered condition sets. Do not call this operation many
  // times with a single condition set in each call.
  void AddConditionSets(const URLMatcherConditionSet::Vector& condition_sets);

  // Removes the listed condition sets. All |condition_set_ids| must be
  // currently registered. This function should be called with large batches
  // of |condition_set_ids| at a time to improve performance.
  void RemoveConditionSets(
      const std::vector<URLMatcherConditionSet::ID>& condition_set_ids);

  // Removes all unused condition sets from the ConditionFactory.
  void ClearUnusedConditionSets();

  // Returns the IDs of all URLMatcherConditionSet that match to this |url|.
  std::set<URLMatcherConditionSet::ID> MatchURL(const GURL& url);

  // Returns the URLMatcherConditionFactory that must be used to create
  // URLMatcherConditionSets for this URLMatcher.
  URLMatcherConditionFactory* condition_factory() {
    return &condition_factory_;
  }

  // Returns true if this object retains no allocated data. Only for debugging.
  bool IsEmpty() const;

 private:
  void UpdateSubstringSetMatcher(bool full_url_conditions);
  void UpdateTriggers();
  void UpdateConditionFactory();
  void UpdateInternalDatastructures();

  URLMatcherConditionFactory condition_factory_;

  // Maps the ID of a URLMatcherConditionSet to the respective
  // URLMatcherConditionSet.
  typedef std::map<URLMatcherConditionSet::ID,
                   scoped_refptr<URLMatcherConditionSet> >
      URLMatcherConditionSets;
  URLMatcherConditionSets url_matcher_condition_sets_;

  // Maps a SubstringPattern ID to the URLMatcherConditions that need to
  // be triggered in case of a SubstringPattern match.
  std::map<SubstringPattern::ID, std::set<URLMatcherConditionSet::ID> >
      substring_match_triggers_;

  SubstringSetMatcher full_url_matcher_;
  SubstringSetMatcher url_component_matcher_;
  std::set<const SubstringPattern*> registered_full_url_patterns_;
  std::set<const SubstringPattern*> registered_url_component_patterns_;

  DISALLOW_COPY_AND_ASSIGN(URLMatcher);
};

}  // namespace extensions

#endif  // CHROME_COMMON_EXTENSIONS_MATCHER_URL_MATCHER_H_
