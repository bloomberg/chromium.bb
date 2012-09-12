// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOCOMPLETE_AUTOCOMPLETE_PROVIDER_H_
#define CHROME_BROWSER_AUTOCOMPLETE_AUTOCOMPLETE_PROVIDER_H_

#include <string>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/string16.h"
#include "chrome/browser/autocomplete/autocomplete_match.h"
#include "chrome/common/metrics/proto/omnibox_event.pb.h"

class AutocompleteInput;
class AutocompleteProviderListener;
class GURL;
class Profile;

typedef std::vector<metrics::OmniboxEventProto_ProviderInfo> ProvidersInfo;

// The AutocompleteProviders each return different kinds of matches,
// such as history or search matches.  These matches are given
// "relevance" scores.  Higher scores are better matches than lower
// scores.  The relevance scores and classes providing the respective
// matches are as listed below.
//
// IMPORTANT CAVEAT: The tables below are NOT COMPLETE.  Developers
// often forget to keep these tables in sync with the code when they
// change scoring algorithms or add new providers.  For example,
// neither the HistoryQuickProvider (which is a provider that appears
// often) nor the ShortcutsProvider are listed here.  For the best
// idea of how scoring works and what providers are affecting which
// queries, play with chrome://omnibox/ for a while.  While the tables
// below may have some utility, nothing compares with first-hand
// investigation and experience.
//
// UNKNOWN input type:
// --------------------------------------------------------------------|-----
// Keyword (non-substituting or in keyword UI mode, exact match)       | 1500
// Extension App (exact match)                                         | 1425
// HistoryURL (good exact or inline autocomplete matches, some inexact)| 1410++
// HistoryURL (intranet url never visited match, some inexact matches) | 1400++
// Search Primary Provider (past query in history within 2 days)       | 1399**
// Search Primary Provider (what you typed)                            | 1300
// HistoryURL (what you typed, some inexact matches)                   | 1200++
// Extension App (inexact match)                                       | 1175*~
// Keyword (substituting, exact match)                                 | 1100
// Search Primary Provider (past query in history older than 2 days)   | 1050--
// HistoryContents (any match in title of starred page)                | 1000++
// HistoryURL (some inexact matches)                                   |  900++
// Search Primary Provider (navigational suggestion)                   |  800++
// HistoryContents (any match in title of nonstarred page)             |  700++
// Search Primary Provider (suggestion)                                |  600++
// Built-in                                                            |  575++
// HistoryContents (any match in body of starred page)                 |  550++
// HistoryContents (any match in body of nonstarred page)              |  500++
// Keyword (inexact match)                                             |  450
// Search Secondary Provider (what you typed)                          |  250
// Search Secondary Provider (past query in history)                   |  200--
// Search Secondary Provider (navigational suggestion)                 |  150++
// Search Secondary Provider (suggestion)                              |  100++
//
// REQUESTED_URL input type:
// --------------------------------------------------------------------|-----
// Keyword (non-substituting or in keyword UI mode, exact match)       | 1500
// Extension App (exact match)                                         | 1425
// HistoryURL (good exact or inline autocomplete matches, some inexact)| 1410++
// HistoryURL (intranet url never visited match, some inexact matches) | 1400++
// Search Primary Provider (past query in history within 2 days)       | 1399**
// HistoryURL (what you typed, some inexact matches)                   | 1200++
// Extension App (inexact match)                                       | 1175*~
// Search Primary Provider (what you typed)                            | 1150
// Keyword (substituting, exact match)                                 | 1100
// Search Primary Provider (past query in history older than 2 days)   | 1050--
// HistoryContents (any match in title of starred page)                | 1000++
// HistoryURL (some inexact matches)                                   |  900++
// Search Primary Provider (navigational suggestion)                   |  800++
// HistoryContents (any match in title of nonstarred page)             |  700++
// Search Primary Provider (suggestion)                                |  600++
// Built-in                                                            |  575++
// HistoryContents (any match in body of starred page)                 |  550++
// HistoryContents (any match in body of nonstarred page)              |  500++
// Keyword (inexact match)                                             |  450
// Search Secondary Provider (what you typed)                          |  250
// Search Secondary Provider (past query in history)                   |  200--
// Search Secondary Provider (navigational suggestion)                 |  150++
// Search Secondary Provider (suggestion)                              |  100++
//
// URL input type:
// --------------------------------------------------------------------|-----
// Keyword (non-substituting or in keyword UI mode, exact match)       | 1500
// Extension App (exact match)                                         | 1425
// HistoryURL (good exact or inline autocomplete matches, some inexact)| 1410++
// HistoryURL (intranet url never visited match, some inexact matches) | 1400++
// HistoryURL (what you typed, some inexact matches)                   | 1200++
// Extension App (inexact match)                                       | 1175*~
// Keyword (substituting, exact match)                                 | 1100
// HistoryURL (some inexact matches)                                   |  900++
// Search Primary Provider (what you typed)                            |  850
// Search Primary Provider (navigational suggestion)                   |  800++
// Search Primary Provider (past query in history)                     |  750--
// Keyword (inexact match)                                             |  700
// Built-in                                                            |  575++
// Search Primary Provider (suggestion)                                |  300++
// Search Secondary Provider (what you typed)                          |  250
// Search Secondary Provider (past query in history)                   |  200--
// Search Secondary Provider (navigational suggestion)                 |  150++
// Search Secondary Provider (suggestion)                              |  100++
//
// QUERY input type:
// --------------------------------------------------------------------|-----
// Search Primary or Secondary (past query in history within 2 days)   | 1599**
// Keyword (non-substituting or in keyword UI mode, exact match)       | 1500
// Keyword (substituting, exact match)                                 | 1450
// Extension App (exact match)                                         | 1425
// Search Primary Provider (past query in history within 2 days)       | 1399**
// Search Primary Provider (what you typed)                            | 1300
// Extension App (inexact match)                                       | 1175*~
// Search Primary Provider (past query in history older than 2 days)   | 1050--
// HistoryContents (any match in title of starred page)                | 1000++
// HistoryURL (inexact match)                                          |  900++
// Search Primary Provider (navigational suggestion)                   |  800++
// HistoryContents (any match in title of nonstarred page)             |  700++
// Search Primary Provider (suggestion)                                |  600++
// HistoryContents (any match in body of starred page)                 |  550++
// HistoryContents (any match in body of nonstarred page)              |  500++
// Keyword (inexact match)                                             |  450
// Search Secondary Provider (what you typed)                          |  250
// Search Secondary Provider (past query in history)                   |  200--
// Search Secondary Provider (navigational suggestion)                 |  150++
// Search Secondary Provider (suggestion)                              |  100++
//
// FORCED_QUERY input type:
// --------------------------------------------------------------------|-----
// Extension App (exact match on title only, not url)                  | 1425
// Search Primary Provider (past query in history within 2 days)       | 1399**
// Search Primary Provider (what you typed)                            | 1300
// Extension App (inexact match on title only, not url)                | 1175*~
// Search Primary Provider (past query in history older than 2 days)   | 1050--
// HistoryContents (any match in title of starred page)                | 1000++
// Search Primary Provider (navigational suggestion)                   |  800++
// HistoryContents (any match in title of nonstarred page)             |  700++
// Search Primary Provider (suggestion)                                |  600++
// HistoryContents (any match in body of starred page)                 |  550++
// HistoryContents (any match in body of nonstarred page)              |  500++
//
// (A search keyword is a keyword with a replacement string; a bookmark keyword
// is a keyword with no replacement string, that is, a shortcut for a URL.)
//
// There are two possible providers for search suggestions. If the user has
// typed a keyword, then the primary provider is the keyword provider and the
// secondary provider is the default provider. If the user has not typed a
// keyword, then the primary provider corresponds to the default provider.
//
// Search providers may supply relevance values along with their results to be
// used in place of client-side calculated values.
//
// The value column gives the ranking returned from the various providers.
// ++: a series of matches with relevance from n up to (n + max_matches).
// --: relevance score falls off over time (discounted 50 points @ 15 minutes,
//     450 points @ two weeks)
// **: relevance score falls off over two days (discounted 99 points after two
//     days).
// *~: Partial matches get a score on a sliding scale from about 575-1125 based
//     on how many times the URL for the Extension App has been typed and how
//     many of the letters match.
//
// A single result provider for the autocomplete system.  Given user input, the
// provider decides what (if any) matches to return, their relevance, and their
// classifications.
class AutocompleteProvider
    : public base::RefCountedThreadSafe<AutocompleteProvider> {
 public:
  // Different AutocompleteProvider implementations.
  enum Type {
    TYPE_BUILTIN          = 1 << 0,
    TYPE_CONTACT          = 1 << 1,
    TYPE_EXTENSION_APP    = 1 << 2,
    TYPE_HISTORY_CONTENTS = 1 << 3,
    TYPE_HISTORY_QUICK    = 1 << 4,
    TYPE_HISTORY_URL      = 1 << 5,
    TYPE_KEYWORD          = 1 << 6,
    TYPE_SEARCH           = 1 << 7,
    TYPE_SHORTCUTS        = 1 << 8,
    TYPE_ZERO_SUGGEST     = 1 << 9,
  };

  AutocompleteProvider(AutocompleteProviderListener* listener,
                       Profile* profile,
                       Type type);

  // Returns a string describing a particular AutocompleteProvider type.
  static const char* TypeToString(Type type);

  // Called to start an autocomplete query.  The provider is responsible for
  // tracking its matches for this query and whether it is done processing the
  // query.  When new matches are available or the provider finishes, it
  // calls the controller's OnProviderUpdate() method.  The controller can then
  // get the new matches using the provider's accessors.
  // Exception: Matches available immediately after starting the query (that
  // is, synchronously) do not cause any notifications to be sent.  The
  // controller is expected to check for these without prompting (since
  // otherwise, starting each provider running would result in a flurry of
  // notifications).
  //
  // Once Stop() has been called, no more notifications should be sent.
  //
  // |minimal_changes| is an optimization that lets the provider do less work
  // when the |input|'s text hasn't changed.  See the body of
  // OmniboxPopupModel::StartAutocomplete().
  virtual void Start(const AutocompleteInput& input, bool minimal_changes) = 0;

  // Called when a provider must not make any more callbacks for the current
  // query. This will be called regardless of whether the provider is already
  // done.  If the provider caches any results, it should clear the cache based
  // on the value of |clear_cached_results|.
  virtual void Stop(bool clear_cached_results);

  // Returns the set of matches for the current query.
  const ACMatches& matches() const { return matches_; }

  // Returns whether the provider is done processing the query.
  bool done() const { return done_; }

  // Returns this provider's type.
  Type type() const { return type_; }

  // Returns a string describing this provider's type.
  const char* GetName() const;

  // Returns the enum equivalent to the name of this provider.
  // TODO(derat): Make metrics use AutocompleteProvider::Type directly, or at
  // least move this method to the metrics directory.
  metrics::OmniboxEventProto_ProviderType AsOmniboxEventProviderType() const;

  // Called to delete a match and the backing data that produced it.  This
  // match should not appear again in this or future queries.  This can only be
  // called for matches the provider marks as deletable.  This should only be
  // called when no query is running.
  // NOTE: Remember to call OnProviderUpdate() if matches_ is updated.
  virtual void DeleteMatch(const AutocompleteMatch& match);

  // Called when an omnibox event log entry is generated.  This gives
  // a provider the opportunity to add diagnostic information to the
  // logs.  A provider is expected to append a single entry of whatever
  // information it wants to |provider_info|.
  virtual void AddProviderInfo(ProvidersInfo* provider_info) const;

  // A convenience function to call net::FormatUrl() with the current set of
  // "Accept Languages" when check_accept_lang is true.  Otherwise, it's called
  // with an empty list.
  string16 StringForURLDisplay(const GURL& url,
                               bool check_accept_lang,
                               bool trim_http) const;

#ifdef UNIT_TEST
  void set_listener(AutocompleteProviderListener* listener) {
    listener_ = listener;
  }
#endif
  // A suggested upper bound for how many matches a provider should return.
  // TODO(pkasting): http://b/1111299 , http://b/933133 This should go away once
  // we have good relevance heuristics; the controller should handle all
  // culling.
  static const size_t kMaxMatches;

 protected:
  friend class base::RefCountedThreadSafe<AutocompleteProvider>;

  virtual ~AutocompleteProvider();

  // Returns whether |input| begins "http:" or "view-source:http:".
  static bool HasHTTPScheme(const string16& input);

  // Updates the starred state of each of the matches in matches_ from the
  // profile's bookmark bar model.
  void UpdateStarredStateOfMatches();

  // The profile associated with the AutocompleteProvider.  Reference is not
  // owned by us.
  Profile* profile_;

  AutocompleteProviderListener* listener_;
  ACMatches matches_;
  bool done_;

  Type type_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AutocompleteProvider);
};

typedef std::vector<AutocompleteProvider*> ACProviders;

#endif  // CHROME_BROWSER_AUTOCOMPLETE_AUTOCOMPLETE_PROVIDER_H_
