// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/matcher/url_matcher.h"

#include <algorithm>
#include <iterator>

#include "base/logging.h"
#include "content/public/common/url_constants.h"
#include "googleurl/src/gurl.h"
#include "googleurl/src/url_canon.h"

namespace extensions {

// This set of classes implement a mapping of URL Component Patterns, such as
// host_prefix, host_suffix, host_equals, ..., etc., to StringPatterns
// for use in substring comparisons.
//
// The idea of this mapping is to reduce the problem of comparing many
// URL Component Patterns against one URL to the problem of searching many
// substrings in one string:
//
// ----------------------                    -----------------
// | URL Query operator | ----translate----> | StringPattern |
// ----------------------                    -----------------
//                                                   ^
//                                                   |
//                                                compare
//                                                   |
//                                                   v
// ----------------------                    -----------------
// | URL to compare     |                    |               |
// | to all URL Query   | ----translate----> | String        |
// | operators          |                    |               |
// ----------------------                    -----------------
//
// The reason for this problem reduction is that there are efficient algorithms
// for searching many substrings in one string (see Aho-Corasick algorithm).
//
// Additionally, some of the same pieces are reused to implement regular
// expression comparisons. The FilteredRE2 implementation for matching many
// regular expressions against one string uses prefiltering, in which a set
// of substrings (derived from the regexes) are first searched for, to reduce
// the number of regular expressions to test; the prefiltering step also
// uses Aho-Corasick.
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
// With this, we can search the StringPatterns in the normalized URL.
//
//
// Case 2: url_{prefix,suffix,equals,contains} searches.
// =====================================================
//
// Step 1: as above, except that
// - the scheme is not removed
// - the port is not removed if it is specified and does not match the default
//   port for the given scheme.
//
// Step 2:
// Translate URL to String and add the following position markers:
// - BU = Beginning of URL
// - EU = End of URL
//
// -> http://www.example.com:8080/index.html?search=foo#first_match becomes
// BU http://www.example.com:8080/index.html?search=foo EU
// -> http://www.example.com:80/index.html?search=foo#first_match becomes
// BU http://www.example.com/index.html?search=foo EU
//
// url_prefix(prefix) = BU prefix
// -> url_prefix("http://www.example") = BU http://www.example
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
//   in host component of original URL.
// -> host_contains("example.co") = example.co
//    followed by gurl.host().find("example.co");
//
// [similarly for path_contains and query_contains].
//
//
// Regular expression matching (url_matches searches)
// ==================================================
//
// This class also supports matching regular expressions (RE2 syntax)
// against full URLs, which are transformed as in case 2.

namespace {

bool IsRegexCriterion(URLMatcherCondition::Criterion criterion) {
  return criterion == URLMatcherCondition::URL_MATCHES;
}

}  // namespace

//
// URLMatcherCondition
//

URLMatcherCondition::URLMatcherCondition()
    : criterion_(HOST_PREFIX),
      string_pattern_(NULL) {}

URLMatcherCondition::~URLMatcherCondition() {}

URLMatcherCondition::URLMatcherCondition(
    Criterion criterion,
    const StringPattern* string_pattern)
    : criterion_(criterion),
      string_pattern_(string_pattern) {}

URLMatcherCondition::URLMatcherCondition(const URLMatcherCondition& rhs)
    : criterion_(rhs.criterion_),
      string_pattern_(rhs.string_pattern_) {}

URLMatcherCondition& URLMatcherCondition::operator=(
    const URLMatcherCondition& rhs) {
  criterion_ = rhs.criterion_;
  string_pattern_ = rhs.string_pattern_;
  return *this;
}

bool URLMatcherCondition::operator<(const URLMatcherCondition& rhs) const {
  if (criterion_ < rhs.criterion_) return true;
  if (criterion_ > rhs.criterion_) return false;
  if (string_pattern_ != NULL && rhs.string_pattern_ != NULL)
    return *string_pattern_ < *rhs.string_pattern_;
  if (string_pattern_ == NULL && rhs.string_pattern_ != NULL) return true;
  // Either string_pattern_ != NULL && rhs.string_pattern_ == NULL,
  // or both are NULL.
  return false;
}

bool URLMatcherCondition::IsFullURLCondition() const {
  // For these criteria the SubstringMatcher needs to be executed on the
  // GURL that is canonicalized with
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

bool URLMatcherCondition::IsRegexCondition() const {
  return IsRegexCriterion(criterion_);
}

bool URLMatcherCondition::IsMatch(
    const std::set<StringPattern::ID>& matching_patterns,
    const GURL& url) const {
  DCHECK(string_pattern_);
  if (!ContainsKey(matching_patterns, string_pattern_->id()))
    return false;
  // The criteria HOST_CONTAINS, PATH_CONTAINS, QUERY_CONTAINS are based on
  // a substring match on the raw URL. In case of a match, we need to verify
  // that the match was found in the correct component of the URL.
  switch (criterion_) {
    case HOST_CONTAINS:
      return url.host().find(string_pattern_->pattern()) !=
          std::string::npos;
    case PATH_CONTAINS:
      return url.path().find(string_pattern_->pattern()) !=
          std::string::npos;
    case QUERY_CONTAINS:
      return url.query().find(string_pattern_->pattern()) !=
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
const char kBeginningOfURL[] = {static_cast<char>(-1), 0};
const char kEndOfDomain[] = {static_cast<char>(-2), 0};
const char kEndOfPath[] = {static_cast<char>(-3), 0};
const char kEndOfURL[] = {static_cast<char>(-4), 0};
}  // namespace

URLMatcherConditionFactory::URLMatcherConditionFactory() : id_counter_(0) {}

URLMatcherConditionFactory::~URLMatcherConditionFactory() {
  STLDeleteElements(&substring_pattern_singletons_);
  STLDeleteElements(&regex_pattern_singletons_);
}

std::string URLMatcherConditionFactory::CanonicalizeURLForComponentSearches(
    const GURL& url) const {
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
  return CreateCondition(URLMatcherCondition::PATH_SUFFIX, suffix + kEndOfPath);
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
  std::string pattern;
  if (!prefix.empty() && prefix[0] == '?')
    pattern = kEndOfPath + prefix;
  else
    pattern = kEndOfPath + ('?' + prefix);

  return CreateCondition(URLMatcherCondition::QUERY_PREFIX, pattern);
}

URLMatcherCondition URLMatcherConditionFactory::CreateQuerySuffixCondition(
    const std::string& suffix) {
  if (!suffix.empty() && suffix[0] == '?') {
    return CreateQueryEqualsCondition(suffix);
  } else {
    return CreateCondition(URLMatcherCondition::QUERY_SUFFIX,
                           suffix + kEndOfURL);
  }
}

URLMatcherCondition URLMatcherConditionFactory::CreateQueryContainsCondition(
    const std::string& str) {
  if (!str.empty() && str[0] == '?')
    return CreateQueryPrefixCondition(str);
  else
    return CreateCondition(URLMatcherCondition::QUERY_CONTAINS, str);
}

URLMatcherCondition URLMatcherConditionFactory::CreateQueryEqualsCondition(
    const std::string& str) {
  std::string pattern;
  if (!str.empty() && str[0] == '?')
    pattern = kEndOfPath + str + kEndOfURL;
  else
    pattern = kEndOfPath + ('?' + str) + kEndOfURL;

  return CreateCondition(URLMatcherCondition::QUERY_EQUALS, pattern);
}

URLMatcherCondition
    URLMatcherConditionFactory::CreateHostSuffixPathPrefixCondition(
    const std::string& host_suffix,
    const std::string& path_prefix) {
  return CreateCondition(URLMatcherCondition::HOST_SUFFIX_PATH_PREFIX,
      host_suffix + kEndOfDomain + path_prefix);
}

URLMatcherCondition
URLMatcherConditionFactory::CreateHostEqualsPathPrefixCondition(
    const std::string& host,
    const std::string& path_prefix) {
  return CreateCondition(URLMatcherCondition::HOST_EQUALS_PATH_PREFIX,
      kBeginningOfURL + CanonicalizeHostname(host) + kEndOfDomain +
      path_prefix);
}

std::string URLMatcherConditionFactory::CanonicalizeURLForFullSearches(
    const GURL& url) const {
  GURL::Replacements replacements;
  replacements.ClearPassword();
  replacements.ClearUsername();
  replacements.ClearRef();
  // Clear port if it is implicit from scheme.
  if (url.has_port()) {
    const std::string& port = url.scheme();
    if (url_canon::DefaultPortForScheme(port.c_str(), port.size()) ==
            url.EffectiveIntPort()) {
      replacements.ClearPort();
    }
  }
  return kBeginningOfURL + url.ReplaceComponents(replacements).spec() +
      kEndOfURL;
}

std::string URLMatcherConditionFactory::CanonicalizeURLForRegexSearches(
    const GURL& url) const {
  GURL::Replacements replacements;
  replacements.ClearPassword();
  replacements.ClearUsername();
  replacements.ClearRef();
  // Clear port if it is implicit from scheme.
  if (url.has_port()) {
    const std::string& port = url.scheme();
    if (url_canon::DefaultPortForScheme(port.c_str(), port.size()) ==
            url.EffectiveIntPort()) {
      replacements.ClearPort();
    }
  }
  return url.ReplaceComponents(replacements).spec();
}

URLMatcherCondition URLMatcherConditionFactory::CreateURLPrefixCondition(
    const std::string& prefix) {
  return CreateCondition(URLMatcherCondition::URL_PREFIX,
      kBeginningOfURL + prefix);
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
  return CreateCondition(URLMatcherCondition::URL_EQUALS,
      kBeginningOfURL + str + kEndOfURL);
}

URLMatcherCondition URLMatcherConditionFactory::CreateURLMatchesCondition(
    const std::string& regex) {
  return CreateCondition(URLMatcherCondition::URL_MATCHES, regex);
}

void URLMatcherConditionFactory::ForgetUnusedPatterns(
      const std::set<StringPattern::ID>& used_patterns) {
  PatternSingletons::iterator i = substring_pattern_singletons_.begin();
  while (i != substring_pattern_singletons_.end()) {
    if (used_patterns.find((*i)->id()) != used_patterns.end()) {
      ++i;
    } else {
      delete *i;
      substring_pattern_singletons_.erase(i++);
    }
  }
  i = regex_pattern_singletons_.begin();
  while (i != regex_pattern_singletons_.end()) {
    if (used_patterns.find((*i)->id()) != used_patterns.end()) {
      ++i;
    } else {
      delete *i;
      regex_pattern_singletons_.erase(i++);
    }
  }
}

bool URLMatcherConditionFactory::IsEmpty() const {
  return substring_pattern_singletons_.empty() &&
      regex_pattern_singletons_.empty();
}

URLMatcherCondition URLMatcherConditionFactory::CreateCondition(
    URLMatcherCondition::Criterion criterion,
    const std::string& pattern) {
  StringPattern search_pattern(pattern, 0);
  PatternSingletons* pattern_singletons =
      IsRegexCriterion(criterion) ? &regex_pattern_singletons_
                                  : &substring_pattern_singletons_;

  PatternSingletons::const_iterator iter =
      pattern_singletons->find(&search_pattern);

  if (iter != pattern_singletons->end()) {
    return URLMatcherCondition(criterion, *iter);
  } else {
    StringPattern* new_pattern =
        new StringPattern(pattern, id_counter_++);
    pattern_singletons->insert(new_pattern);
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

bool URLMatcherConditionFactory::StringPatternPointerCompare::operator()(
    StringPattern* lhs,
    StringPattern* rhs) const {
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
    const std::set<StringPattern::ID>& matching_patterns,
    const GURL& url) const {
  for (Conditions::const_iterator i = conditions_.begin();
       i != conditions_.end(); ++i) {
    if (!i->IsMatch(matching_patterns, url))
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

std::set<URLMatcherConditionSet::ID> URLMatcher::MatchURL(
    const GURL& url) const {
  // Find all IDs of StringPatterns that match |url|.
  // See URLMatcherConditionFactory for the canonicalization of URLs and the
  // distinction between full url searches and url component searches.
  std::set<StringPattern::ID> matches;
  full_url_matcher_.Match(
      condition_factory_.CanonicalizeURLForFullSearches(url), &matches);
  url_component_matcher_.Match(
      condition_factory_.CanonicalizeURLForComponentSearches(url), &matches);
  regex_set_matcher_.Match(
      condition_factory_.CanonicalizeURLForRegexSearches(url), &matches);

  // Calculate all URLMatcherConditionSets for which all URLMatcherConditions
  // were fulfilled.
  std::set<URLMatcherConditionSet::ID> result;
  for (std::set<StringPattern::ID>::const_iterator i = matches.begin();
       i != matches.end(); ++i) {
    // For each URLMatcherConditionSet there is exactly one condition
    // registered in substring_match_triggers_. This means that the following
    // logic tests each URLMatcherConditionSet exactly once if it can be
    // completely fulfilled.
    StringPatternTriggers::const_iterator triggered_condition_sets_iter =
        substring_match_triggers_.find(*i);
    if (triggered_condition_sets_iter == substring_match_triggers_.end())
      continue;  // Not all substring matches are triggers for a condition set.
    const std::set<URLMatcherConditionSet::ID>& condition_sets =
        triggered_condition_sets_iter->second;
    for (std::set<URLMatcherConditionSet::ID>::const_iterator j =
         condition_sets.begin(); j != condition_sets.end(); ++j) {
      URLMatcherConditionSets::const_iterator condition_set_iter =
          url_matcher_condition_sets_.find(*j);
      DCHECK(condition_set_iter != url_matcher_condition_sets_.end());
      if (condition_set_iter->second->IsMatch(matches, url))
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
  std::set<const StringPattern*> new_patterns;
  for (URLMatcherConditionSets::const_iterator condition_set_iter =
      url_matcher_condition_sets_.begin();
      condition_set_iter != url_matcher_condition_sets_.end();
      ++condition_set_iter) {
    const URLMatcherConditionSet::Conditions& conditions =
        condition_set_iter->second->conditions();
    for (URLMatcherConditionSet::Conditions::const_iterator condition_iter =
         conditions.begin(); condition_iter != conditions.end();
         ++condition_iter) {
      // If we are called to process Full URL searches, ignore others, and
      // vice versa. (Regex conditions are updated in UpdateRegexSetMatcher.)
      if (!condition_iter->IsRegexCondition() &&
          full_url_conditions == condition_iter->IsFullURLCondition())
        new_patterns.insert(condition_iter->string_pattern());
    }
  }

  // This is the set of patterns that were registered before this function
  // is called.
  std::set<const StringPattern*>& registered_patterns =
      full_url_conditions ? registered_full_url_patterns_
                          : registered_url_component_patterns_;

  // Add all patterns that are in new_patterns but not in registered_patterns.
  std::vector<const StringPattern*> patterns_to_register;
  std::set_difference(
      new_patterns.begin(), new_patterns.end(),
      registered_patterns.begin(), registered_patterns.end(),
      std::back_inserter(patterns_to_register));

  // Remove all patterns that are in registered_patterns but not in
  // new_patterns.
  std::vector<const StringPattern*> patterns_to_unregister;
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

void URLMatcher::UpdateRegexSetMatcher() {
  std::vector<const StringPattern*> new_patterns;

  for (URLMatcherConditionSets::const_iterator condition_set_iter =
      url_matcher_condition_sets_.begin();
      condition_set_iter != url_matcher_condition_sets_.end();
      ++condition_set_iter) {
    const URLMatcherConditionSet::Conditions& conditions =
        condition_set_iter->second->conditions();
    for (URLMatcherConditionSet::Conditions::const_iterator condition_iter =
         conditions.begin(); condition_iter != conditions.end();
         ++condition_iter) {
      if (condition_iter->IsRegexCondition())
        new_patterns.push_back(condition_iter->string_pattern());
    }
  }

  // Start over from scratch. We can't really do better than this, since the
  // FilteredRE2 backend doesn't support incremental updates.
  regex_set_matcher_.ClearPatterns();
  regex_set_matcher_.AddPatterns(new_patterns);
}

void URLMatcher::UpdateTriggers() {
  // Count substring pattern frequencies.
  std::map<StringPattern::ID, size_t> substring_pattern_frequencies;
  for (URLMatcherConditionSets::const_iterator condition_set_iter =
      url_matcher_condition_sets_.begin();
      condition_set_iter != url_matcher_condition_sets_.end();
      ++condition_set_iter) {
    const URLMatcherConditionSet::Conditions& conditions =
        condition_set_iter->second->conditions();
    for (URLMatcherConditionSet::Conditions::const_iterator condition_iter =
         conditions.begin(); condition_iter != conditions.end();
         ++condition_iter) {
      const StringPattern* pattern = condition_iter->string_pattern();
      substring_pattern_frequencies[pattern->id()]++;
    }
  }

  // Update trigger conditions: Determine for each URLMatcherConditionSet which
  // URLMatcherCondition contains a StringPattern that occurs least
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
    StringPattern::ID trigger = condition_iter->string_pattern()->id();
    // We skip the first element in the following loop.
    ++condition_iter;
    for (; condition_iter != conditions.end(); ++condition_iter) {
      StringPattern::ID current_id =
          condition_iter->string_pattern()->id();
      if (substring_pattern_frequencies[trigger] >
          substring_pattern_frequencies[current_id]) {
        trigger = current_id;
      }
    }
    substring_match_triggers_[trigger].insert(condition_set_iter->second->id());
  }
}

void URLMatcher::UpdateConditionFactory() {
  std::set<StringPattern::ID> used_patterns;
  for (URLMatcherConditionSets::const_iterator condition_set_iter =
      url_matcher_condition_sets_.begin();
      condition_set_iter != url_matcher_condition_sets_.end();
      ++condition_set_iter) {
    const URLMatcherConditionSet::Conditions& conditions =
        condition_set_iter->second->conditions();
    for (URLMatcherConditionSet::Conditions::const_iterator condition_iter =
         conditions.begin(); condition_iter != conditions.end();
         ++condition_iter) {
      used_patterns.insert(condition_iter->string_pattern()->id());
    }
  }
  condition_factory_.ForgetUnusedPatterns(used_patterns);
}

void URLMatcher::UpdateInternalDatastructures() {
  UpdateSubstringSetMatcher(false);
  UpdateSubstringSetMatcher(true);
  UpdateRegexSetMatcher();
  UpdateTriggers();
  UpdateConditionFactory();
}

}  // namespace extensions
