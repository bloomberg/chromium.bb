// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/matcher/url_matcher.h"

#include <algorithm>
#include <iterator>

#include "base/logging.h"
#include "googleurl/src/gurl.h"

namespace extensions {

// This set of classes implement a mapping of URL Component Patterns, such as
// host_prefix, host_suffix, host_equals, ..., etc., to SubstringPatterns.
//
// The idea of this mapping is to reduce the problem of comparing many
// URL Component Patterns against one URL to the problem of searching many
// substrings in one string:
//
// ----------------------                    --------------------
// | URL Query operator | ----translate----> | SubstringPattern |
// ----------------------                    --------------------
//                                                    ^
//                                                    |
//                                                 compare
//                                                    |
//                                                    v
// ----------------------                    --------------------
// | URL to compare     |                    |                  |
// | to all URL Query   | ----translate----> | String           |
// | operators          |                    |                  |
// ----------------------                    --------------------
//
// The reason for this problem reduction is that there are efficient algorithms
// for searching many substrings in one string (see Aho-Corasick algorithm).
//
// Case 1: {host,path,query}_{prefix,suffix,equals} searches.
// ==========================================================
//
// For searches in this class, we normalize URLs as follows:
//
// Step 1:
// Remove scheme, port and segment from URL:
// -> http://www.example.com:8080/index.html?search=foo#first_match becomes
//    www.example.com/index.html?search=foo
//
// We remove the scheme and port number because they can be checked later
// in a secondary filter step. We remove the segment (the #... part) because
// this is not guaranteed to be ASCII-7 encoded.
//
// Step 2:
// Translate URL to String and add the following position markers:
// - BU = Beginning of URL
// - ED = End of Domain
// - EP = End of Path
// - EU = End of URL
// Furthermore, the hostname is canonicalized to start with a ".".
//
// Position markers are represented as characters >127, which are therefore
// guaranteed not to be part of the ASCII-7 encoded URL character set.
//
// -> www.example.com/index.html?search=foo becomes
// BU .www.example.com ED /index.html EP ?search=foo EU
//
// -> www.example.com/index.html becomes
// BU .www.example.com ED /index.html EP EU
//
// Step 3:
// Translate URL Component Patterns as follows:
//
// host_prefix(prefix) = BU add_missing_dot_prefix(prefix)
// -> host_prefix("www.example") = BU .www.example
//
// host_suffix(suffix) = suffix ED
// -> host_suffix("example.com") = example.com ED
// -> host_suffix(".example.com") = .example.com ED
//
// host_equals(domain) = BU add_missing_dot_prefix(domain) ED
// -> host_equals("www.example.com") = BU .www.example.com ED
//
// Similarly for path query parameters ({path, query}_{prefix, suffix, equals}).
//
// With this, we can search the SubstringPatterns in the normalized URL.
//
//
// Case 2: url_{prefix,suffix,equals,contains} searches.
// =====================================================
//
// Step 1: as above
//
// Step 2:
// Translate URL to String and add the following position markers:
// - BU = Beginning of URL
// - EU = End of URL
// Furthermore, the hostname is canonicalized to start with a ".".
//
// -> www.example.com/index.html?search=foo becomes
// BU .www.example.com/index.html?search=foo EU
//
// url_prefix(prefix) = BU add_missing_dot_prefix(prefix)
// -> url_prefix("www.example") = BU .www.example
//
// url_contains(substring) = substring
// -> url_contains("index") = index
//
//
// Case 3: {host,path,query}_contains searches.
// ============================================
//
// These kinds of searches are not supported directly but can be derived
// by a combination of a url_contains() query followed by an explicit test:
//
// host_contains(str) = url_contains(str) followed by test whether str occurs
//   in host comonent of original URL.
// -> host_contains("example.co") = example.co
//    followed by gurl.host().find("example.co");
//
// [similarly for path_contains and query_contains].


//
// URLMatcherCondition
//

URLMatcherCondition::URLMatcherCondition()
    : criterion_(HOST_PREFIX),
      substring_pattern_(NULL) {}

URLMatcherCondition::~URLMatcherCondition() {}

URLMatcherCondition::URLMatcherCondition(
    Criterion criterion,
    const SubstringPattern* substring_pattern)
    : criterion_(criterion),
      substring_pattern_(substring_pattern) {}

URLMatcherCondition::URLMatcherCondition(const URLMatcherCondition& rhs)
    : criterion_(rhs.criterion_),
      substring_pattern_(rhs.substring_pattern_) {}

URLMatcherCondition& URLMatcherCondition::operator=(
    const URLMatcherCondition& rhs) {
  criterion_ = rhs.criterion_;
  substring_pattern_ = rhs.substring_pattern_;
  return *this;
}

bool URLMatcherCondition::operator<(const URLMatcherCondition& rhs) const {
  if (criterion_ < rhs.criterion_) return true;
  if (criterion_ > rhs.criterion_) return false;
  if (substring_pattern_ != NULL && rhs.substring_pattern_ != NULL)
    return *substring_pattern_ < *rhs.substring_pattern_;
  if (substring_pattern_ == NULL && rhs.substring_pattern_ != NULL) return true;
  // Either substring_pattern_ != NULL && rhs.substring_pattern_ == NULL,
  // or both are NULL.
  return false;
}

bool URLMatcherCondition::IsFullURLCondition() const {
  // For these criteria the SubstringMatcher needs to be executed on the
  // GURL that is canonlizaliced with
  // URLMatcherConditionFactory::CanonicalizeURLForFullSearches.
  switch (criterion_) {
    case HOST_CONTAINS:
    case PATH_CONTAINS:
    case QUERY_CONTAINS:
    case URL_PREFIX:
    case URL_SUFFIX:
    case URL_CONTAINS:
    case URL_EQUALS:
      return true;
    default:
      break;
  }
  return false;
}

bool URLMatcherCondition::IsMatch(
    const std::set<SubstringPattern::ID>& matching_substring_patterns,
    const GURL& url) const {
  DCHECK(substring_pattern_);
  if (matching_substring_patterns.find(substring_pattern_->id()) ==
      matching_substring_patterns.end())
    return false;
  // The criteria HOST_CONTAINS, PATH_CONTAINS, QUERY_CONTAINS are based on
  // a substring match on the raw URL. In case of a match, we need to verify
  // that the match was found in the correct component of the URL.
  switch (criterion_) {
    case HOST_CONTAINS:
      return url.host().find(substring_pattern_->pattern()) !=
          std::string::npos;
    case PATH_CONTAINS:
      return url.path().find(substring_pattern_->pattern()) !=
          std::string::npos;
    case QUERY_CONTAINS:
      return url.query().find(substring_pattern_->pattern()) !=
          std::string::npos;
    default:
      break;
  }
  return true;
}

//
// URLMatcherConditionFactory
//

namespace {
// These are symbols that are not contained in 7-bit ASCII used in GURLs.
const char kBeginningOfURL[] = {-1, 0};
const char kEndOfDomain[] = {-2, 0};
const char kEndOfPath[] = {-3, 0};
const char kEndOfURL[] = {-4, 0};
}  // namespace

URLMatcherConditionFactory::URLMatcherConditionFactory() : id_counter_(0) {}

URLMatcherConditionFactory::~URLMatcherConditionFactory() {
  STLDeleteElements(&pattern_singletons_);
}

std::string URLMatcherConditionFactory::CanonicalizeURLForComponentSearches(
    const GURL& url) {
  return kBeginningOfURL + CanonicalizeHostname(url.host()) + kEndOfDomain +
      url.path() + kEndOfPath + (url.has_query() ? "?" + url.query() : "") +
      kEndOfURL;
}

URLMatcherCondition URLMatcherConditionFactory::CreateHostPrefixCondition(
    const std::string& prefix) {
  return CreateCondition(URLMatcherCondition::HOST_PREFIX,
      kBeginningOfURL + CanonicalizeHostname(prefix));
}

URLMatcherCondition URLMatcherConditionFactory::CreateHostSuffixCondition(
    const std::string& suffix) {
  return CreateCondition(URLMatcherCondition::HOST_SUFFIX,
      suffix + kEndOfDomain);
}

URLMatcherCondition URLMatcherConditionFactory::CreateHostContainsCondition(
    const std::string& str) {
  return CreateCondition(URLMatcherCondition::HOST_CONTAINS, str);
}

URLMatcherCondition URLMatcherConditionFactory::CreateHostEqualsCondition(
    const std::string& str) {
  return CreateCondition(URLMatcherCondition::HOST_EQUALS,
      kBeginningOfURL + CanonicalizeHostname(str) + kEndOfDomain);
}

URLMatcherCondition URLMatcherConditionFactory::CreatePathPrefixCondition(
    const std::string& prefix) {
  return CreateCondition(URLMatcherCondition::PATH_PREFIX,
      kEndOfDomain + prefix);
}

URLMatcherCondition URLMatcherConditionFactory::CreatePathSuffixCondition(
    const std::string& suffix) {
  return CreateCondition(URLMatcherCondition::HOST_SUFFIX, suffix + kEndOfPath);
}

URLMatcherCondition URLMatcherConditionFactory::CreatePathContainsCondition(
    const std::string& str) {
  return CreateCondition(URLMatcherCondition::PATH_CONTAINS, str);
}

URLMatcherCondition URLMatcherConditionFactory::CreatePathEqualsCondition(
    const std::string& str) {
  return CreateCondition(URLMatcherCondition::PATH_EQUALS,
      kEndOfDomain + str + kEndOfPath);
}

URLMatcherCondition URLMatcherConditionFactory::CreateQueryPrefixCondition(
    const std::string& prefix) {
  return CreateCondition(URLMatcherCondition::QUERY_PREFIX,
      kEndOfPath + prefix);
}

URLMatcherCondition URLMatcherConditionFactory::CreateQuerySuffixCondition(
    const std::string& suffix) {
  return CreateCondition(URLMatcherCondition::QUERY_SUFFIX, suffix + kEndOfURL);
}

URLMatcherCondition URLMatcherConditionFactory::CreateQueryContainsCondition(
    const std::string& str) {
  return CreateCondition(URLMatcherCondition::QUERY_CONTAINS, str);
}

URLMatcherCondition URLMatcherConditionFactory::CreateQueryEqualsCondition(
    const std::string& str) {
  return CreateCondition(URLMatcherCondition::QUERY_EQUALS,
      kEndOfPath + str + kEndOfURL);
}

URLMatcherCondition
    URLMatcherConditionFactory::CreateHostSuffixPathPrefixCondition(
    const std::string& host_suffix,
    const std::string& path_prefix) {
  return CreateCondition(URLMatcherCondition::HOST_SUFFIX_PATH_PREFIX,
      host_suffix + kEndOfDomain + path_prefix);
}

std::string URLMatcherConditionFactory::CanonicalizeURLForFullSearches(
    const GURL& url) {
  return kBeginningOfURL + CanonicalizeHostname(url.host()) + url.path() +
      (url.has_query() ? "?" + url.query() : "") + kEndOfURL;
}

URLMatcherCondition URLMatcherConditionFactory::CreateURLPrefixCondition(
    const std::string& prefix) {
  return CreateCondition(URLMatcherCondition::URL_PREFIX,
      kBeginningOfURL + CanonicalizeHostname(prefix));
}

URLMatcherCondition URLMatcherConditionFactory::CreateURLSuffixCondition(
    const std::string& suffix) {
  return CreateCondition(URLMatcherCondition::URL_SUFFIX, suffix + kEndOfURL);
}

URLMatcherCondition URLMatcherConditionFactory::CreateURLContainsCondition(
    const std::string& str) {
  return CreateCondition(URLMatcherCondition::URL_CONTAINS, str);
}

URLMatcherCondition URLMatcherConditionFactory::CreateURLEqualsCondition(
    const std::string& str) {
  return CreateCondition(URLMatcherCondition::QUERY_EQUALS,
      kBeginningOfURL + CanonicalizeHostname(str) + kEndOfURL);
}

void URLMatcherConditionFactory::ForgetUnusedPatterns(
      const std::set<SubstringPattern::ID>& used_patterns) {
  PatternSingletons::iterator i = pattern_singletons_.begin();
  while (i != pattern_singletons_.end()) {
    if (used_patterns.find((*i)->id()) != used_patterns.end()) {
      ++i;
    } else {
      delete *i;
      pattern_singletons_.erase(i++);
    }
  }
}

bool URLMatcherConditionFactory::IsEmpty() const {
  return pattern_singletons_.empty();
}

URLMatcherCondition URLMatcherConditionFactory::CreateCondition(
    URLMatcherCondition::Criterion criterion,
    const std::string& pattern) {
  SubstringPattern search_pattern(pattern, 0);
  PatternSingletons::const_iterator iter =
      pattern_singletons_.find(&search_pattern);
  if (iter != pattern_singletons_.end()) {
    return URLMatcherCondition(criterion, *iter);
  } else {
    SubstringPattern* new_pattern =
        new SubstringPattern(pattern, id_counter_++);
    pattern_singletons_.insert(new_pattern);
    return URLMatcherCondition(criterion, new_pattern);
  }
}

std::string URLMatcherConditionFactory::CanonicalizeHostname(
    const std::string& hostname) const {
  if (!hostname.empty() && hostname[0] == '.')
    return hostname;
  else
    return "." + hostname;
}

bool URLMatcherConditionFactory::SubstringPatternPointerCompare::operator()(
    SubstringPattern* lhs,
    SubstringPattern* rhs) const {
  if (lhs == NULL && rhs != NULL) return true;
  if (lhs != NULL && rhs != NULL)
    return lhs->pattern() < rhs->pattern();
  // Either both are NULL or only rhs is NULL.
  return false;
}

//
// URLMatcherSchemeFilter
//

URLMatcherSchemeFilter::URLMatcherSchemeFilter(const std::string& filter)
    : filters_(1) {
  filters_.push_back(filter);
}

URLMatcherSchemeFilter::URLMatcherSchemeFilter(
    const std::vector<std::string>& filters)
    : filters_(filters) {}

URLMatcherSchemeFilter::~URLMatcherSchemeFilter() {}

bool URLMatcherSchemeFilter::IsMatch(const GURL& url) const {
  return std::find(filters_.begin(), filters_.end(), url.scheme()) !=
      filters_.end();
}

//
// URLMatcherPortFilter
//

URLMatcherPortFilter::URLMatcherPortFilter(
    const std::vector<URLMatcherPortFilter::Range>& ranges)
    : ranges_(ranges) {}

URLMatcherPortFilter::~URLMatcherPortFilter() {}

bool URLMatcherPortFilter::IsMatch(const GURL& url) const {
  int port = url.EffectiveIntPort();
  for (std::vector<Range>::const_iterator i = ranges_.begin();
       i != ranges_.end(); ++i) {
    if (i->first <= port && port <= i->second)
      return true;
  }
  return false;
}

// static
URLMatcherPortFilter::Range URLMatcherPortFilter::CreateRange(int from,
                                                              int to) {
  return Range(from, to);
}

// static
URLMatcherPortFilter::Range URLMatcherPortFilter::CreateRange(int port) {
  return Range(port, port);
}

//
// URLMatcherConditionSet
//

URLMatcherConditionSet::~URLMatcherConditionSet() {}

URLMatcherConditionSet::URLMatcherConditionSet(
    ID id,
    const Conditions& conditions)
    : id_(id),
      conditions_(conditions) {}

URLMatcherConditionSet::URLMatcherConditionSet(
    ID id,
    const Conditions& conditions,
    scoped_ptr<URLMatcherSchemeFilter> scheme_filter,
    scoped_ptr<URLMatcherPortFilter> port_filter)
    : id_(id),
      conditions_(conditions),
      scheme_filter_(scheme_filter.Pass()),
      port_filter_(port_filter.Pass()) {}

bool URLMatcherConditionSet::IsMatch(
    const std::set<SubstringPattern::ID>& matching_substring_patterns,
    const GURL& url) const {
  for (Conditions::const_iterator i = conditions_.begin();
       i != conditions_.end(); ++i) {
    if (!i->IsMatch(matching_substring_patterns, url))
      return false;
  }
  if (scheme_filter_.get() && !scheme_filter_->IsMatch(url))
    return false;
  if (port_filter_.get() && !port_filter_->IsMatch(url))
    return false;
  return true;
}


//
// URLMatcher
//

URLMatcher::URLMatcher() {}

URLMatcher::~URLMatcher() {}

void URLMatcher::AddConditionSets(
    const URLMatcherConditionSet::Vector& condition_sets) {
  for (URLMatcherConditionSet::Vector::const_iterator i =
       condition_sets.begin(); i != condition_sets.end(); ++i) {
    DCHECK(url_matcher_condition_sets_.find((*i)->id()) ==
        url_matcher_condition_sets_.end());
    url_matcher_condition_sets_[(*i)->id()] = *i;
  }
  UpdateInternalDatastructures();
}

void URLMatcher::RemoveConditionSets(
    const std::vector<URLMatcherConditionSet::ID>& condition_set_ids) {
  for (std::vector<URLMatcherConditionSet::ID>::const_iterator i =
       condition_set_ids.begin(); i != condition_set_ids.end(); ++i) {
    DCHECK(url_matcher_condition_sets_.find(*i) !=
        url_matcher_condition_sets_.end());
    url_matcher_condition_sets_.erase(*i);
  }
  UpdateInternalDatastructures();
}

void URLMatcher::ClearUnusedConditionSets() {
  UpdateConditionFactory();
}

std::set<URLMatcherConditionSet::ID> URLMatcher::MatchURL(const GURL& url) {
  // Find all IDs of SubstringPatterns that match |url|.
  // See URLMatcherConditionFactory for the canonicalization of URLs and the
  // distinction between full url searches and url component searches.
  std::set<SubstringPattern::ID> matches;
  full_url_matcher_.Match(
      condition_factory_.CanonicalizeURLForFullSearches(url), &matches);
  url_component_matcher_.Match(
      condition_factory_.CanonicalizeURLForComponentSearches(url), &matches);

  // Calculate all URLMatcherConditionSets for which all URLMatcherConditions
  // were fulfilled.
  std::set<URLMatcherConditionSet::ID> result;
  for (std::set<SubstringPattern::ID>::const_iterator i = matches.begin();
       i != matches.end(); ++i) {
    // For each URLMatcherConditionSet there is exactly one condition
    // registered in substring_match_triggers_. This means that the following
    // logic tests each URLMatcherConditionSet exactly once if it can be
    // completely fulfilled.
    std::set<URLMatcherConditionSet::ID>& condition_sets =
        substring_match_triggers_[*i];
    for (std::set<URLMatcherConditionSet::ID>::const_iterator j =
         condition_sets.begin(); j != condition_sets.end(); ++j) {
      if (url_matcher_condition_sets_[*j]->IsMatch(matches, url))
        result.insert(*j);
    }
  }

  return result;
}

bool URLMatcher::IsEmpty() const {
  return condition_factory_.IsEmpty() &&
      url_matcher_condition_sets_.empty() &&
      substring_match_triggers_.empty() &&
      full_url_matcher_.IsEmpty() &&
      url_component_matcher_.IsEmpty() &&
      registered_full_url_patterns_.empty() &&
      registered_url_component_patterns_.empty();
}

void URLMatcher::UpdateSubstringSetMatcher(bool full_url_conditions) {
  // The purpose of |full_url_conditions| is just that we need to execute
  // the same logic once for Full URL searches and once for URL Component
  // searches (see URLMatcherConditionFactory).

  // Determine which patterns need to be registered when this function
  // terminates.
  std::set<const SubstringPattern*> new_patterns;
  for (URLMatcherConditionSets::const_iterator condition_set_iter =
      url_matcher_condition_sets_.begin();
      condition_set_iter != url_matcher_condition_sets_.end();
      ++condition_set_iter) {
    const URLMatcherConditionSet::Conditions& conditions =
        condition_set_iter->second->conditions();
    for (URLMatcherConditionSet::Conditions::const_iterator condition_iter =
         conditions.begin(); condition_iter != conditions.end();
         ++condition_iter) {
      // If we are called to process Full URL searches, ignore all others,
      // and vice versa.
      if (full_url_conditions == condition_iter->IsFullURLCondition())
        new_patterns.insert(condition_iter->substring_pattern());
    }
  }

  // This is the set of patterns that were registered before this function
  // is called.
  std::set<const SubstringPattern*>& registered_patterns =
      full_url_conditions ? registered_full_url_patterns_
                          : registered_url_component_patterns_;

  // Add all patterns that are in new_patterns but not in registered_patterns.
  std::vector<const SubstringPattern*> patterns_to_register;
  std::set_difference(
      new_patterns.begin(), new_patterns.end(),
      registered_patterns.begin(), registered_patterns.end(),
      std::back_inserter(patterns_to_register));

  // Remove all patterns that are in registered_patterns but not in
  // new_patterns.
  std::vector<const SubstringPattern*> patterns_to_unregister;
  std::set_difference(
      registered_patterns.begin(), registered_patterns.end(),
      new_patterns.begin(), new_patterns.end(),
      std::back_inserter(patterns_to_unregister));

  // Update the SubstringSetMatcher.
  SubstringSetMatcher& url_matcher =
      full_url_conditions ? full_url_matcher_ : url_component_matcher_;
  url_matcher.RegisterAndUnregisterPatterns(patterns_to_register,
                                            patterns_to_unregister);

  // Update the set of registered_patterns for the next time this function
  // is being called.
  registered_patterns.swap(new_patterns);
}

void URLMatcher::UpdateTriggers() {
  // Count substring pattern frequencies.
  std::map<SubstringPattern::ID, size_t> substring_pattern_frequencies;
  for (URLMatcherConditionSets::const_iterator condition_set_iter =
      url_matcher_condition_sets_.begin();
      condition_set_iter != url_matcher_condition_sets_.end();
      ++condition_set_iter) {
    const URLMatcherConditionSet::Conditions& conditions =
        condition_set_iter->second->conditions();
    for (URLMatcherConditionSet::Conditions::const_iterator condition_iter =
         conditions.begin(); condition_iter != conditions.end();
         ++condition_iter) {
      const SubstringPattern* pattern = condition_iter->substring_pattern();
      substring_pattern_frequencies[pattern->id()]++;
    }
  }

  // Update trigger conditions: Determine for each URLMatcherConditionSet which
  // URLMatcherCondition contains a SubstringPattern that occurs least
  // frequently in this URLMatcher. We assume that this condition is very
  // specific and occurs rarely in URLs. If a match occurs for this
  // URLMatcherCondition, we want to test all other URLMatcherCondition in the
  // respective URLMatcherConditionSet as well to see whether the entire
  // URLMatcherConditionSet is considered matching.
  substring_match_triggers_.clear();
  for (URLMatcherConditionSets::const_iterator condition_set_iter =
      url_matcher_condition_sets_.begin();
      condition_set_iter != url_matcher_condition_sets_.end();
      ++condition_set_iter) {
    const URLMatcherConditionSet::Conditions& conditions =
        condition_set_iter->second->conditions();
    if (conditions.empty())
      continue;
    URLMatcherConditionSet::Conditions::const_iterator condition_iter =
        conditions.begin();
    SubstringPattern::ID trigger = condition_iter->substring_pattern()->id();
    // We skip the first element in the following loop.
    ++condition_iter;
    for (; condition_iter != conditions.end(); ++condition_iter) {
      SubstringPattern::ID current_id =
          condition_iter->substring_pattern()->id();
      if (substring_pattern_frequencies[trigger] >
          substring_pattern_frequencies[current_id]) {
        trigger = current_id;
      }
    }
    substring_match_triggers_[trigger].insert(condition_set_iter->second->id());
  }
}

void URLMatcher::UpdateConditionFactory() {
  std::set<SubstringPattern::ID> used_patterns;
  for (URLMatcherConditionSets::const_iterator condition_set_iter =
      url_matcher_condition_sets_.begin();
      condition_set_iter != url_matcher_condition_sets_.end();
      ++condition_set_iter) {
    const URLMatcherConditionSet::Conditions& conditions =
        condition_set_iter->second->conditions();
    for (URLMatcherConditionSet::Conditions::const_iterator condition_iter =
         conditions.begin(); condition_iter != conditions.end();
         ++condition_iter) {
      used_patterns.insert(condition_iter->substring_pattern()->id());
    }
  }
  condition_factory_.ForgetUnusedPatterns(used_patterns);
}

void URLMatcher::UpdateInternalDatastructures() {
  UpdateSubstringSetMatcher(false);
  UpdateSubstringSetMatcher(true);
  UpdateTriggers();
  UpdateConditionFactory();
}

}  // namespace extensions
