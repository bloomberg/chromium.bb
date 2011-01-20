// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOCOMPLETE_AUTOCOMPLETE_H_
#define CHROME_BROWSER_AUTOCOMPLETE_AUTOCOMPLETE_H_
#pragma once

#include <string>
#include <vector>

#include "base/logging.h"
#include "base/ref_counted.h"
#include "base/timer.h"
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
// The AutocompleteProviders each return different kinds of matches, such as
// history or search matches.  These matches are given "relevance" scores.
// Higher scores are better matches than lower scores.  The relevance scores and
// classes providing the respective matches are as follows:
//
// UNKNOWN input type:
// --------------------------------------------------------------------|-----
// Keyword (non-substituting or in keyword UI mode, exact match)       | 1500
// HistoryURL (exact or inline autocomplete match)                     | 1400
// Search Primary Provider (past query in history within 2 days)       | 1399**
// Search Primary Provider (what you typed)                            | 1300
// HistoryURL (what you typed)                                         | 1200
// Keyword (substituting, exact match)                                 | 1100
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
// REQUESTED_URL input type:
// --------------------------------------------------------------------|-----
// Keyword (non-substituting or in keyword UI mode, exact match)       | 1500
// HistoryURL (exact or inline autocomplete match)                     | 1400
// Search Primary Provider (past query in history within 2 days)       | 1399**
// HistoryURL (what you typed)                                         | 1200
// Search Primary Provider (what you typed)                            | 1150
// Keyword (substituting, exact match)                                 | 1100
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
// URL input type:
// --------------------------------------------------------------------|-----
// Keyword (non-substituting or in keyword UI mode, exact match)       | 1500
// HistoryURL (exact or inline autocomplete match)                     | 1400
// HistoryURL (what you typed)                                         | 1200
// Keyword (substituting, exact match)                                 | 1100
// HistoryURL (inexact match)                                          |  900++
// Search Primary Provider (what you typed)                            |  850
// Search Primary Provider (navigational suggestion)                   |  800++
// Search Primary Provider (past query in history)                     |  750--
// Keyword (inexact match)                                             |  700
// Search Primary Provider (suggestion)                                |  300++
// Search Secondary Provider (what you typed)                          |  250
// Search Secondary Provider (past query in history)                   |  200--
// Search Secondary Provider (navigational suggestion)                 |  150++
// Search Secondary Provider (suggestion)                              |  100++
//
// QUERY input type:
// --------------------------------------------------------------------|-----
// Keyword (non-substituting or in keyword UI mode, exact match)       | 1500
// Keyword (substituting, exact match)                                 | 1450
// HistoryURL (exact or inline autocomplete match)                     | 1400
// Search Primary Provider (past query in history within 2 days)       | 1399**
// Search Primary Provider (what you typed)                            | 1300
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
// Search Primary Provider (past query in history within 2 days)       | 1399**
// Search Primary Provider (what you typed)                            | 1300
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
// --: relevance score falls off over two days (discounted 99 points after two
//     days).

class AutocompleteInput;
struct AutocompleteMatch;
class AutocompleteProvider;
class AutocompleteResult;
class AutocompleteController;
class HistoryContentsProvider;
class Profile;
class SearchProvider;
class TemplateURL;

typedef std::vector<AutocompleteMatch> ACMatches;
typedef std::vector<AutocompleteProvider*> ACProviders;

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

  AutocompleteInput();
  AutocompleteInput(const std::wstring& text,
                    const std::wstring& desired_tld,
                    bool prevent_inline_autocomplete,
                    bool prefer_keyword,
                    bool allow_exact_keyword_match,
                    bool synchronous_only);
  ~AutocompleteInput();

  // If type is |FORCED_QUERY| and |text| starts with '?', it is removed.
  static void RemoveForcedQueryStringIfNecessary(Type type, std::wstring* text);

  // Converts |type| to a string representation.  Used in logging.
  static std::string TypeToString(Type type);

  // Parses |text| and returns the type of input this will be interpreted as.
  // The components of the input are stored in the output parameter |parts|, if
  // it is non-NULL. The scheme is stored in |scheme| if it is non-NULL. The
  // canonicalized URL is stored in |canonicalized_url|; however, this URL is
  // not guaranteed to be valid, especially if the parsed type is, e.g., QUERY.
  static Type Parse(const std::wstring& text,
                    const std::wstring& desired_tld,
                    url_parse::Parsed* parts,
                    std::wstring* scheme,
                    GURL* canonicalized_url);

  // Parses |text| and fill |scheme| and |host| by the positions of them.
  // The results are almost as same as the result of Parse(), but if the scheme
  // is view-source, this function returns the positions of scheme and host
  // in the URL qualified by "view-source:" prefix.
  static void ParseForEmphasizeComponents(const std::wstring& text,
                                          const std::wstring& desired_tld,
                                          url_parse::Component* scheme,
                                          url_parse::Component* host);

  // Code that wants to format URLs with a format flag including
  // net::kFormatUrlOmitTrailingSlashOnBareHostname risk changing the meaning if
  // the result is then parsed as AutocompleteInput.  Such code can call this
  // function with the URL and its formatted string, and it will return a
  // formatted string with the same meaning as the original URL (i.e. it will
  // re-append a slash if necessary).
  static std::wstring FormattedStringWithEquivalentMeaning(
      const GURL& url,
      const std::wstring& formatted_url);

  // User-provided text to be completed.
  const std::wstring& text() const { return text_; }

  // Use of this setter is risky, since no other internal state is updated
  // besides |text_|.  Only callers who know that they're not changing the
  // type/scheme/etc. should use this.
  void set_text(const std::wstring& text) { text_ = text; }

  // User's desired TLD, if one is not already present in the text to
  // autocomplete.  When this is non-empty, it also implies that "www." should
  // be prepended to the domain where possible.  This should not have a leading
  // '.' (use "com" instead of ".com").
  const std::wstring& desired_tld() const { return desired_tld_; }

  // The type of input supplied.
  Type type() const { return type_; }

  // Returns parsed URL components.
  const url_parse::Parsed& parts() const { return parts_; }

  // The scheme parsed from the provided text; only meaningful when type_ is
  // URL.
  const std::wstring& scheme() const { return scheme_; }

  // The input as an URL to navigate to, if possible.
  const GURL& canonicalized_url() const { return canonicalized_url_; }

  // Returns whether inline autocompletion should be prevented.
  bool prevent_inline_autocomplete() const {
    return prevent_inline_autocomplete_;
  }

  // Returns the value of |prevent_inline_autocomplete| supplied to the
  // constructor. This differs from the value returned by
  // |prevent_inline_autocomplete()| if the input contained trailing whitespace.
  bool initial_prevent_inline_autocomplete() const {
    return initial_prevent_inline_autocomplete_;
  }

  // Returns whether, given an input string consisting solely of a substituting
  // keyword, we should score it like a non-substituting keyword.
  bool prefer_keyword() const { return prefer_keyword_; }

  // Returns whether this input is allowed to be treated as an exact
  // keyword match.  If not, the default result is guaranteed not to be a
  // keyword search, even if the input is "<keyword> <search string>".
  bool allow_exact_keyword_match() const { return allow_exact_keyword_match_; }

  // Returns whether providers should avoid scheduling asynchronous work.  If
  // this is true, providers should stop after returning all the
  // synchronously-available matches.  This also means any in-progress
  // asynchronous work should be canceled, so no later callbacks are fired.
  bool synchronous_only() const { return synchronous_only_; }

  // operator==() by another name.
  bool Equals(const AutocompleteInput& other) const;

  // Resets all internal variables to the null-constructed state.
  void Clear();

 private:
  std::wstring text_;
  std::wstring desired_tld_;
  Type type_;
  url_parse::Parsed parts_;
  std::wstring scheme_;
  GURL canonicalized_url_;
  bool initial_prevent_inline_autocomplete_;
  bool prevent_inline_autocomplete_;
  bool prefer_keyword_;
  bool allow_exact_keyword_match_;
  bool synchronous_only_;
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

  // Invoked when the profile changes.
  // NOTE: Do not access any previous Profile* at this point as it may have
  // already been deleted.
  void SetProfile(Profile* profile);

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
  const char* name() const { return name_; }

  // Called to delete a match and the backing data that produced it.  This
  // match should not appear again in this or future queries.  This can only be
  // called for matches the provider marks as deletable.  This should only be
  // called when no query is running.
  // NOTE: Remember to call OnProviderUpdate() if matches_ is updated.
  virtual void DeleteMatch(const AutocompleteMatch& match);

  // A suggested upper bound for how many matches a provider should return.
  // TODO(pkasting): http://b/1111299 , http://b/933133 This should go away once
  // we have good relevance heuristics; the controller should handle all
  // culling.
  static const size_t kMaxMatches;

 protected:
  friend class base::RefCountedThreadSafe<AutocompleteProvider>;

  virtual ~AutocompleteProvider();

  // Returns whether |input| begins "http:" or "view-source:http:".
  static bool HasHTTPScheme(const std::wstring& input);

  // Updates the starred state of each of the matches in matches_ from the
  // profile's bookmark bar model.
  void UpdateStarredStateOfMatches();

  // A convenience function to call net::FormatUrl() with the current set of
  // "Accept Languages" when check_accept_lang is true.  Otherwise, it's called
  // with an empty list.
  std::wstring StringForURLDisplay(const GURL& url,
                                   bool check_accept_lang,
                                   bool trim_http) const;

  // The profile associated with the AutocompleteProvider.  Reference is not
  // owned by us.
  Profile* profile_;

  ACProviderListener* listener_;
  ACMatches matches_;
  bool done_;

  // The name of this provider.  Used for logging.
  const char* name_;

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

  AutocompleteResult();
  ~AutocompleteResult();

  // operator=() by another name.
  void CopyFrom(const AutocompleteResult& rhs);

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

#ifndef NDEBUG
  // Does a data integrity check on this result.
  void Validate() const;
#endif

  // Max number of matches we'll show from the various providers.
  static const size_t kMaxMatches;

 private:
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
  explicit AutocompleteController(Profile* profile);
#ifdef UNIT_TEST
  explicit AutocompleteController(const ACProviders& providers)
      : providers_(providers),
        search_provider_(NULL),
        updated_latest_result_(false),
        delay_interval_has_passed_(false),
        have_committed_during_this_query_(false),
        done_(true) {
  }
#endif
  ~AutocompleteController();

  // Invoked when the profile changes. This forwards the call down to all
  // the AutocompleteProviders.
  void SetProfile(Profile* profile);

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
  // in a page and telling the browser to search for it or navigate to it.

  // If |synchronous_only| is true, the controller asks the providers to only
  // return matches which are synchronously available, which should mean that
  // all providers will be done immediately.
  //
  // The controller will fire
  // AUTOCOMPLETE_CONTROLLER_SYNCHRONOUS_MATCHES_AVAILABLE from inside this
  // call, and unless the query is stopped, will fire at least one (and perhaps
  // more) AUTOCOMPLETE_CONTROLLER_RESULT_UPDATED later as more matches come in
  // (even if the query completes synchronously).  Listeners should use the
  // result set provided in the accompanying Details object to update
  // themselves.
  void Start(const std::wstring& text,
             const std::wstring& desired_tld,
             bool prevent_inline_autocomplete,
             bool prefer_keyword,
             bool allow_exact_keyword_match,
             bool synchronous_only);

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

  // Commits the results for the current query if they've never been committed.
  // This is used by the popup to ensure it's not showing an out-of-date query.
  void CommitIfQueryHasNeverBeenCommitted();

  SearchProvider* search_provider() const { return search_provider_; }

  // Getters
  const AutocompleteInput& input() const { return input_; }
  const AutocompleteResult& result() const { return result_; }
  // This next is temporary and should go away when
  // AutocompletePopup::InfoForCurrentSelection() moves to the controller.
  const AutocompleteResult& latest_result() const { return latest_result_; }
  bool done() const { return done_ && !update_delay_timer_.IsRunning(); }

  // From AutocompleteProvider::Listener
  virtual void OnProviderUpdate(bool updated_matches);

 private:
  // Updates |latest_result_| and |done_| to reflect the current provider state.
  // Resets timers and fires notifications as necessary.  |is_synchronous_pass|
  // is true only when Start() is calling this to get the synchronous result.
  void UpdateLatestResult(bool is_synchronous_pass);

  // Callback for when |max_delay_timer_| fires; this notes that the delay
  // interval has passed and commits the result, if it's changed.
  void DelayTimerFired();

  // Copies |latest_result_| to |result_| and notifies observers of updates.
  // |notify_default_match| should normally be true; if it's false, we don't
  // send an AUTOCOMPLETE_CONTROLLER_DEFAULT_MATCH_UPDATED notification.  This
  // is a hack to avoid updating the edit with out-of-date data.
  // TODO(pkasting): Don't hardcode assumptions about who listens to these
  // notificiations.
  void CommitResult(bool notify_default_match);

  // Updates |done_| to be accurate with respect to current providers' statuses.
  void CheckIfDone();

  // A list of all providers.
  ACProviders providers_;

  SearchProvider* search_provider_;

  // Input passed to Start.
  AutocompleteInput input_;

  // Data from the autocomplete query.
  AutocompleteResult result_;

  // The latest result available from the autocomplete providers.  This may be
  // different than |result_| if we've gotten matches from our providers that we
  // haven't yet shown the user.  If there aren't yet as many matches as in
  // |result|, we'll wait to display these in hopes of minimizing flicker in GUI
  // observers.
  AutocompleteResult latest_result_;

  // True if |latest_result_| has been updated since it was last committed to
  // |result_|.  Used to determine whether we need to commit any changes.
  bool updated_latest_result_;

  // True when it's been at least one interval of the delay timer since we
  // committed any updates.  This is used to allow a new update to be committed
  // immediately.
  //
  // NOTE: This can never be true when |have_committed_during_this_query_| is
  // false (except transiently while processing the timer firing).
  bool delay_interval_has_passed_;

  // True when we've committed a result set at least once during this query.
  // When this is false, we commit immediately when |done_| is set, since there
  // are no more updates to come and thus no possible flicker due to committing
  // immediately.
  //
  // NOTE: This can never be false when |delay_interval_has_passed_| is true
  // (except transiently while processing the timer firing).
  bool have_committed_during_this_query_;

  // True if a query is not currently running.
  bool done_;

  // Timer that tracks how long it's been since the last time we sent our
  // observers a new result set.  This is used both to enforce a lower bound on
  // the delay between most commits (to reduce flicker), and ensure that updates
  // eventually get committed no matter what delays occur between them or how
  // fast or continuously the user is typing.
  base::RepeatingTimer<AutocompleteController> update_delay_timer_;

  DISALLOW_COPY_AND_ASSIGN(AutocompleteController);
};

// AutocompleteLog ------------------------------------------------------------

// The data to log (via the metrics service) when the user selects an item
// from the omnibox popup.
struct AutocompleteLog {
  AutocompleteLog(std::wstring text,
                  AutocompleteInput::Type input_type,
                  size_t selected_index,
                  size_t inline_autocompleted_length,
                  const AutocompleteResult& result)
      : text(text),
        input_type(input_type),
        selected_index(selected_index),
        inline_autocompleted_length(inline_autocompleted_length),
        result(result) {
  }
  // The user's input text in the omnibox.
  std::wstring text;
  // The detected type of the user's input.
  AutocompleteInput::Type input_type;
  // Selected index (if selected) or -1 (AutocompletePopupModel::kNoMatch).
  size_t selected_index;
  // Inline autocompleted length (if displayed).
  size_t inline_autocompleted_length;
  // Result set.
  const AutocompleteResult& result;
};

#endif  // CHROME_BROWSER_AUTOCOMPLETE_AUTOCOMPLETE_H_
