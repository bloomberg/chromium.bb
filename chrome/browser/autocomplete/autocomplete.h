// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOCOMPLETE_AUTOCOMPLETE_H_
#define CHROME_BROWSER_AUTOCOMPLETE_AUTOCOMPLETE_H_
#pragma once

#include <map>
#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/string16.h"
#include "base/time.h"
#include "base/timer.h"
#include "chrome/browser/sessions/session_id.h"
#include "chrome/common/metrics/proto/omnibox_event.pb.h"
#include "googleurl/src/gurl.h"
#include "googleurl/src/url_parse.h"

// The AutocompleteController is the center of the autocomplete system.  A
// class creates an instance of the controller, which in turn creates a set of
// AutocompleteProviders to serve it.  The owning class can ask the controller
// to Start() a query; the controller in turn passes this call down to the
// providers, each of which keeps track of its own matches and whether it has
// finished processing the query.  When a provider gets more matches or finishes
// processing, it notifies the controller, which merges the combined matches
// together and makes the result available to interested observers.
//
// The owner may also cancel the current query by calling Stop(), which the
// controller will in turn communicate to all the providers.  No callbacks will
// happen after a request has been stopped.
//
// IMPORTANT: There is NO THREAD SAFETY built into this portion of the
// autocomplete system.  All calls to and from the AutocompleteController should
// happen on the same thread.  AutocompleteProviders are responsible for doing
// their own thread management when they need to return matches asynchronously.
//
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
// The value column gives the ranking returned from the various providers.
// ++: a series of matches with relevance from n up to (n + max_matches).
// --: relevance score falls off over time (discounted 50 points @ 15 minutes,
//     450 points @ two weeks)
// **: relevance score falls off over two days (discounted 99 points after two
//     days).
// *~: Partial matches get a score on a sliding scale from about 575-1125 based
//     on how many times the URL for the Extension App has been typed and how
//     many of the letters match.

class AutocompleteController;
class AutocompleteControllerDelegate;
class AutocompleteInput;
struct AutocompleteMatch;
class AutocompleteProvider;
class AutocompleteResult;
class KeywordProvider;
class OmniboxUIHandler;
class Profile;
struct ProviderInfo;
class SearchProvider;
class TemplateURL;

typedef std::vector<AutocompleteMatch> ACMatches;
typedef std::vector<AutocompleteProvider*> ACProviders;
typedef std::vector<metrics::OmniboxEventProto_ProviderInfo> ProvidersInfo;

// AutocompleteInput ----------------------------------------------------------

// The user input for an autocomplete query.  Allows copying.
class AutocompleteInput {
 public:
  // Note that the type below may be misleading.  For example, "http:/" alone
  // cannot be opened as a URL, so it is marked as a QUERY; yet the user
  // probably intends to type more and have it eventually become a URL, so we
  // need to make sure we still run it through inline autocomplete.
  enum Type {
    INVALID,        // Empty input
    UNKNOWN,        // Valid input whose type cannot be determined
    REQUESTED_URL,  // Input autodetected as UNKNOWN, which the user wants to
                    // treat as an URL by specifying a desired_tld
    URL,            // Input autodetected as a URL
    QUERY,          // Input autodetected as a query
    FORCED_QUERY,   // Input forced to be a query by an initial '?'
  };

  // Enumeration of the possible match query types. Callers who only need some
  // of the matches for a particular input can get answers more quickly by
  // specifying that upfront.
  enum MatchesRequested {
    // Only the best match in the whole result set matters.  Providers should at
    // most return synchronously-available matches, and if possible do even less
    // work, so that it's safe to ask for these repeatedly in the course of one
    // higher-level "synchronous" query.
    BEST_MATCH,

    // Only synchronous matches should be returned.
    SYNCHRONOUS_MATCHES,

    // All matches should be fetched.
    ALL_MATCHES,
  };

  AutocompleteInput();
  AutocompleteInput(const string16& text,
                    const string16& desired_tld,
                    bool prevent_inline_autocomplete,
                    bool prefer_keyword,
                    bool allow_exact_keyword_match,
                    MatchesRequested matches_requested);
  ~AutocompleteInput();

  // If type is |FORCED_QUERY| and |text| starts with '?', it is removed.
  static void RemoveForcedQueryStringIfNecessary(Type type, string16* text);

  // Converts |type| to a string representation.  Used in logging.
  static std::string TypeToString(Type type);

  // Parses |text| and returns the type of input this will be interpreted as.
  // The components of the input are stored in the output parameter |parts|, if
  // it is non-NULL. The scheme is stored in |scheme| if it is non-NULL. The
  // canonicalized URL is stored in |canonicalized_url|; however, this URL is
  // not guaranteed to be valid, especially if the parsed type is, e.g., QUERY.
  static Type Parse(const string16& text,
                    const string16& desired_tld,
                    url_parse::Parsed* parts,
                    string16* scheme,
                    GURL* canonicalized_url);

  // Parses |text| and fill |scheme| and |host| by the positions of them.
  // The results are almost as same as the result of Parse(), but if the scheme
  // is view-source, this function returns the positions of scheme and host
  // in the URL qualified by "view-source:" prefix.
  static void ParseForEmphasizeComponents(const string16& text,
                                          const string16& desired_tld,
                                          url_parse::Component* scheme,
                                          url_parse::Component* host);

  // Code that wants to format URLs with a format flag including
  // net::kFormatUrlOmitTrailingSlashOnBareHostname risk changing the meaning if
  // the result is then parsed as AutocompleteInput.  Such code can call this
  // function with the URL and its formatted string, and it will return a
  // formatted string with the same meaning as the original URL (i.e. it will
  // re-append a slash if necessary).
  static string16 FormattedStringWithEquivalentMeaning(
      const GURL& url,
      const string16& formatted_url);

  // Returns the number of non-empty components in |parts| besides the host.
  static int NumNonHostComponents(const url_parse::Parsed& parts);

  // User-provided text to be completed.
  const string16& text() const { return text_; }

  // Use of this setter is risky, since no other internal state is updated
  // besides |text_| and |parts_|.  Only callers who know that they're not
  // changing the type/scheme/etc. should use this.
  void UpdateText(const string16& text, const url_parse::Parsed& parts);

  // User's desired TLD, if one is not already present in the text to
  // autocomplete.  When this is non-empty, it also implies that "www." should
  // be prepended to the domain where possible.  This should not have a leading
  // '.' (use "com" instead of ".com").
  const string16& desired_tld() const { return desired_tld_; }

  // The type of input supplied.
  Type type() const { return type_; }

  // Returns parsed URL components.
  const url_parse::Parsed& parts() const { return parts_; }

  // The scheme parsed from the provided text; only meaningful when type_ is
  // URL.
  const string16& scheme() const { return scheme_; }

  // The input as an URL to navigate to, if possible.
  const GURL& canonicalized_url() const { return canonicalized_url_; }

  // Returns whether inline autocompletion should be prevented.
  bool prevent_inline_autocomplete() const {
    return prevent_inline_autocomplete_;
  }

  // Returns whether, given an input string consisting solely of a substituting
  // keyword, we should score it like a non-substituting keyword.
  bool prefer_keyword() const { return prefer_keyword_; }

  // Returns whether this input is allowed to be treated as an exact
  // keyword match.  If not, the default result is guaranteed not to be a
  // keyword search, even if the input is "<keyword> <search string>".
  bool allow_exact_keyword_match() const { return allow_exact_keyword_match_; }

  // See description of enum for details.
  MatchesRequested matches_requested() const { return matches_requested_; }

  // operator==() by another name.
  bool Equals(const AutocompleteInput& other) const;

  // Resets all internal variables to the null-constructed state.
  void Clear();

 private:
  string16 text_;
  string16 desired_tld_;
  Type type_;
  url_parse::Parsed parts_;
  string16 scheme_;
  GURL canonicalized_url_;
  bool prevent_inline_autocomplete_;
  bool prefer_keyword_;
  bool allow_exact_keyword_match_;
  MatchesRequested matches_requested_;
};

// AutocompleteProvider -------------------------------------------------------

// A single result provider for the autocomplete system.  Given user input, the
// provider decides what (if any) matches to return, their relevance, and their
// classifications.
class AutocompleteProvider
    : public base::RefCountedThreadSafe<AutocompleteProvider> {
 public:
  class ACProviderListener {
   public:
    // Called by a provider as a notification that something has changed.
    // |updated_matches| should be true iff the matches have changed in some
    // way (they may not have changed if, for example, the provider did an
    // asynchronous query to get more matches, came up with none, and is now
    // giving up).
    //
    // NOTE: Providers MUST only call this method while processing asynchronous
    // queries.  Do not call this for a synchronous query.
    //
    // NOTE: There's no parameter to tell the listener _which_ provider is
    // calling it.  Because the AutocompleteController (the typical listener)
    // doesn't cache the providers' individual matches locally, it has to get
    // them all again when this is called anyway, so such a parameter wouldn't
    // actually be useful.
    virtual void OnProviderUpdate(bool updated_matches) = 0;

   protected:
    virtual ~ACProviderListener();
  };

  AutocompleteProvider(ACProviderListener* listener,
                       Profile* profile,
                       const char* name);

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
  // AutocompletePopupModel::StartAutocomplete().
  virtual void Start(const AutocompleteInput& input,
                     bool minimal_changes) = 0;

  // Called when a provider must not make any more callbacks for the current
  // query. This will be called regardless of whether the provider is already
  // done.
  virtual void Stop();

  // Returns the set of matches for the current query.
  const ACMatches& matches() const { return matches_; }

  // Returns whether the provider is done processing the query.
  bool done() const { return done_; }

  // Returns the name of this provider.
  const std::string& name() const { return name_; }
  // Returns the enum equivalent to the name of this provider.
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

#ifdef UNIT_TEST
  void set_listener(ACProviderListener* listener) { listener_ = listener; }
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

  // A convenience function to call net::FormatUrl() with the current set of
  // "Accept Languages" when check_accept_lang is true.  Otherwise, it's called
  // with an empty list.
  string16 StringForURLDisplay(const GURL& url,
                               bool check_accept_lang,
                               bool trim_http) const;

  // The profile associated with the AutocompleteProvider.  Reference is not
  // owned by us.
  Profile* profile_;

  ACProviderListener* listener_;
  ACMatches matches_;
  bool done_;

  // The name of this provider.  Used for logging.
  std::string name_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AutocompleteProvider);
};

typedef AutocompleteProvider::ACProviderListener ACProviderListener;

// AutocompleteResult ---------------------------------------------------------

// All matches from all providers for a particular query.  This also tracks
// what the default match should be if the user doesn't manually select another
// match.
class AutocompleteResult {
 public:
  typedef ACMatches::const_iterator const_iterator;
  typedef ACMatches::iterator iterator;

  // The "Selection" struct is the information we need to select the same match
  // in one result set that was selected in another.
  struct Selection {
    Selection()
        : provider_affinity(NULL),
          is_history_what_you_typed_match(false) {
    }

    // Clear the selection entirely.
    void Clear();

    // True when the selection is empty.
    bool empty() const {
      return destination_url.is_empty() && !provider_affinity &&
          !is_history_what_you_typed_match;
    }

    // The desired destination URL.
    GURL destination_url;

    // The desired provider.  If we can't find a match with the specified
    // |destination_url|, we'll use the best match from this provider.
    const AutocompleteProvider* provider_affinity;

    // True when this is the HistoryURLProvider's "what you typed" match.  This
    // can't be tracked using |destination_url| because its URL changes on every
    // keystroke, so if this is set, we'll preserve the selection by simply
    // choosing the new "what you typed" entry and ignoring |destination_url|.
    bool is_history_what_you_typed_match;
  };

  // Max number of matches we'll show from the various providers.
  static const size_t kMaxMatches;

  // The lowest score a match can have and still potentially become the default
  // match for the result set.
  static const int kLowestDefaultScore;

  AutocompleteResult();
  ~AutocompleteResult();

  // operator=() by another name.
  void CopyFrom(const AutocompleteResult& rhs);

  // Copies matches from |old_matches| to provide a consistant result set. See
  // comments in code for specifics.
  void CopyOldMatches(const AutocompleteInput& input,
                      const AutocompleteResult& old_matches);

  // Adds a single match. The match is inserted at the appropriate position
  // based on relevancy and display order. This is ONLY for use after
  // SortAndCull() has been invoked, and preserves default_match_.
  void AddMatch(const AutocompleteMatch& match);

  // Adds a new set of matches to the result set.  Does not re-sort.
  void AppendMatches(const ACMatches& matches);

  // Removes duplicates, puts the list in sorted order and culls to leave only
  // the best kMaxMatches matches.  Sets the default match to the best match
  // and updates the alternate nav URL.
  void SortAndCull(const AutocompleteInput& input);

  // Returns true if at least one match was copied from the last result.
  bool HasCopiedMatches() const;

  // Vector-style accessors/operators.
  size_t size() const;
  bool empty() const;
  const_iterator begin() const;
  iterator begin();
  const_iterator end() const;
  iterator end();

  // Returns the match at the given index.
  const AutocompleteMatch& match_at(size_t index) const;

  // Get the default match for the query (not necessarily the first).  Returns
  // end() if there is no default match.
  const_iterator default_match() const { return default_match_; }

  GURL alternate_nav_url() const { return alternate_nav_url_; }

  // Clears the matches for this result set.
  void Reset();

  void Swap(AutocompleteResult* other);

#ifndef NDEBUG
  // Does a data integrity check on this result.
  void Validate() const;
#endif

 private:
  typedef std::map<AutocompleteProvider*, ACMatches> ProviderToMatches;

#if defined(OS_ANDROID)
  // iterator::difference_type is not defined in the STL that we compile with on
  // Android.
  typedef int matches_difference_type;
#else
  typedef ACMatches::iterator::difference_type matches_difference_type;
#endif

  // Populates |provider_to_matches| from |matches_|.
  void BuildProviderToMatches(ProviderToMatches* provider_to_matches) const;

  // Returns true if |matches| contains a match with the same destination as
  // |match|.
  static bool HasMatchByDestination(const AutocompleteMatch& match,
                                    const ACMatches& matches);

  // Copies matches into this result. |old_matches| gives the matches from the
  // last result, and |new_matches| the results from this result.
  void MergeMatchesByProvider(const ACMatches& old_matches,
                              const ACMatches& new_matches);

  ACMatches matches_;

  const_iterator default_match_;

  // The "alternate navigation URL", if any, for this result set.  This is a URL
  // to try offering as a navigational option in case the user navigated to the
  // URL of the default match but intended something else.  For example, if the
  // user's local intranet contains site "foo", and the user types "foo", we
  // default to searching for "foo" when the user may have meant to navigate
  // there.  In cases like this, the default match will point to the "search for
  // 'foo'" result, and this will contain "http://foo/".
  GURL alternate_nav_url_;

  DISALLOW_COPY_AND_ASSIGN(AutocompleteResult);
};

// AutocompleteController -----------------------------------------------------

// The coordinator for autocomplete queries, responsible for combining the
// matches from a series of providers into one AutocompleteResult.
class AutocompleteController : public ACProviderListener {
 public:
  // Used to indicate an index that is not selected in a call to Update().
  static const int kNoItemSelected;

  // Normally, you will call the first constructor.  Unit tests can use the
  // second to set the providers to some known testing providers.  The default
  // providers will be overridden and the controller will take ownership of the
  // providers, Release()ing them on destruction.
  AutocompleteController(Profile* profile,
                         AutocompleteControllerDelegate* delegate);
#ifdef UNIT_TEST
  AutocompleteController(const ACProviders& providers, Profile* profile)
      : delegate_(NULL),
        providers_(providers),
        keyword_provider_(NULL),
        search_provider_(NULL),
        done_(true),
        in_start_(false),
        profile_(profile) {
  }
#endif
  ~AutocompleteController();

  // Starts an autocomplete query, which continues until all providers are
  // done or the query is Stop()ed.  It is safe to Start() a new query without
  // Stop()ing the previous one.
  //
  // See AutocompleteInput::desired_tld() for meaning of |desired_tld|.
  //
  // |prevent_inline_autocomplete| is true if the generated result set should
  // not require inline autocomplete for the default match.  This is difficult
  // to explain in the abstract; the practical use case is that after the user
  // deletes text in the edit, the HistoryURLProvider should make sure not to
  // promote a match requiring inline autocomplete too highly.
  //
  // |prefer_keyword| should be true when the keyword UI is onscreen; this will
  // bias the autocomplete result set toward the keyword provider when the input
  // string is a bare keyword.
  //
  // |allow_exact_keyword_match| should be false when triggering keyword mode on
  // the input string would be surprising or wrong, e.g. when highlighting text
  // in a page and telling the browser to search for it or navigate to it. This
  // parameter only applies to substituting keywords.

  // If |matches_requested| is BEST_MATCH or SYNCHRONOUS_MATCHES the controller
  // asks the providers to only return matches which are synchronously
  // available, which should mean that all providers will be done immediately.
  //
  // The controller calls AutocompleteControllerDelegate::OnResultChanged() from
  // inside this call at least once. If matches are available later on that
  // result in changing the result set the delegate is notified again. When the
  // controller is done the notification AUTOCOMPLETE_CONTROLLER_RESULT_READY is
  // sent.
  void Start(const string16& text,
             const string16& desired_tld,
             bool prevent_inline_autocomplete,
             bool prefer_keyword,
             bool allow_exact_keyword_match,
             AutocompleteInput::MatchesRequested matches_requested);

  // Cancels the current query, ensuring there will be no future notifications
  // fired.  If new matches have come in since the most recent notification was
  // fired, they will be discarded.
  //
  // If |clear_result| is true, the controller will also erase the result set.
  void Stop(bool clear_result);

  // Asks the relevant provider to delete |match|, and ensures observers are
  // notified of resulting changes immediately.  This should only be called when
  // no query is running.
  void DeleteMatch(const AutocompleteMatch& match);

  // Removes any entries that were copied from the last result. This is used by
  // the popup to ensure it's not showing an out-of-date query.
  void ExpireCopiedEntries();

#ifdef UNIT_TEST
  void set_search_provider(SearchProvider* provider) {
    search_provider_ = provider;
  }
  void set_keyword_provider(KeywordProvider* provider) {
    keyword_provider_ = provider;
  }
#endif
  SearchProvider* search_provider() const { return search_provider_; }
  KeywordProvider* keyword_provider() const { return keyword_provider_; }

  // Getters
  const AutocompleteInput& input() const { return input_; }
  const AutocompleteResult& result() const { return result_; }
  bool done() const { return done_; }
  const ACProviders* providers() const { return &providers_; }

  // From AutocompleteProvider::Listener
  virtual void OnProviderUpdate(bool updated_matches);

  // Called when an omnibox event log entry is generated.
  // Populates provider_info with diagnostic information about the status
  // of various providers.  In turn, calls
  // AutocompleteProvider::AddProviderInfo() so each provider can add
  // provider-specific information, information we want to log for a
  // particular provider but not others.
  void AddProvidersInfo(ProvidersInfo* provider_info) const;

 private:
  friend class AutocompleteProviderTest;
  FRIEND_TEST_ALL_PREFIXES(AutocompleteProviderTest,
                           RedundantKeywordsIgnoredInResult);

  // Updates |result_| to reflect the current provider state.  Resets timers and
  // fires notifications as necessary.  |is_synchronous_pass| is true only when
  // Start() is calling this to get the synchronous result.
  void UpdateResult(bool is_synchronous_pass);

  // Updates |result| to populate each match's |associated_keyword| if that
  // match can show a keyword hint.  |result| should be sorted by
  // relevance before this is called.
  void UpdateAssociatedKeywords(AutocompleteResult* result);

  // For each group of contiguous matches from the same TemplateURL, show the
  // provider name as a description on the first match in the group.
  void UpdateKeywordDescriptions(AutocompleteResult* result);

  // Calls AutocompleteControllerDelegate::OnResultChanged() and if done sends
  // AUTOCOMPLETE_CONTROLLER_RESULT_READY.
  void NotifyChanged(bool notify_default_match);

  // Updates |done_| to be accurate with respect to current providers' statuses.
  void CheckIfDone();

  // Starts the expire timer.
  void StartExpireTimer();

  AutocompleteControllerDelegate* delegate_;

  // A list of all providers.
  ACProviders providers_;

  KeywordProvider* keyword_provider_;

  SearchProvider* search_provider_;

  // Input passed to Start.
  AutocompleteInput input_;

  // Data from the autocomplete query.
  AutocompleteResult result_;

  // Timer used to remove any matches copied from the last result. When run
  // invokes |ExpireCopiedEntries|.
  base::OneShotTimer<AutocompleteController> expire_timer_;

  // True if a query is not currently running.
  bool done_;

  // Are we in Start()? This is used to avoid updating |result_| and sending
  // notifications until Start() has been invoked on all providers.
  bool in_start_;

  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(AutocompleteController);
};

// AutocompleteLog ------------------------------------------------------------

// The data to log (via the metrics service) when the user selects an item
// from the omnibox popup.
struct AutocompleteLog {
  AutocompleteLog(
      const string16& text,
      bool just_deleted_text,
      AutocompleteInput::Type input_type,
      size_t selected_index,
      SessionID::id_type tab_id,
      metrics::OmniboxEventProto::PageClassification
          current_page_classification,
      base::TimeDelta elapsed_time_since_user_first_modified_omnibox,
      size_t inline_autocompleted_length,
      const AutocompleteResult& result);
  ~AutocompleteLog();
  // The user's input text in the omnibox.
  string16 text;
  // Whether the user deleted text immediately before selecting an omnibox
  // suggestion.  This is usually the result of pressing backspace or delete.
  bool just_deleted_text;
  // The detected type of the user's input.
  AutocompleteInput::Type input_type;
  // Selected index (if selected) or -1 (AutocompletePopupModel::kNoMatch).
  size_t selected_index;
  // ID of the tab the selected autocomplete suggestion was opened in.
  // Set to -1 if we haven't yet determined the destination tab.
  SessionID::id_type tab_id;
  // The type of page (e.g., new tab page, regular web page) that the
  // user was viewing before going somewhere with the omnibox.
  metrics::OmniboxEventProto::PageClassification current_page_classification;
  // The amount of time since the user first began modifying the text
  // in the omnibox.  If at some point after modifying the text, the
  // user reverts the modifications (thus seeing the current web
  // page's URL again), then writes in the omnibox again, this time
  // delta should be computed starting from the second series of
  // modifications.  If we somehow skipped the logic to record
  // the time the user began typing (this should only happen in
  // unit tests), this elapsed time is set to -1 milliseconds.
  base::TimeDelta elapsed_time_since_user_first_modified_omnibox;
  // Inline autocompleted length (if displayed).
  size_t inline_autocompleted_length;
  // Result set.
  const AutocompleteResult& result;
  // Diagnostic information from providers.  See
  // AutocompleteController::AddProvidersInfo() and
  // AutocompleteProvider::AddProviderInfo() above.
  ProvidersInfo providers_info;
};

#endif  // CHROME_BROWSER_AUTOCOMPLETE_AUTOCOMPLETE_H_
