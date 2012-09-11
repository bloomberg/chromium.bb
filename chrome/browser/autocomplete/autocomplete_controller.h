// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOCOMPLETE_AUTOCOMPLETE_CONTROLLER_H_
#define CHROME_BROWSER_AUTOCOMPLETE_AUTOCOMPLETE_CONTROLLER_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/string16.h"
#include "base/timer.h"
#include "chrome/browser/autocomplete/autocomplete_input.h"
#include "chrome/browser/autocomplete/autocomplete_provider.h"
#include "chrome/browser/autocomplete/autocomplete_provider_listener.h"
#include "chrome/browser/autocomplete/autocomplete_result.h"

class AutocompleteControllerDelegate;
class KeywordProvider;
class Profile;
class SearchProvider;
class ZeroSuggestProvider;

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
// The coordinator for autocomplete queries, responsible for combining the
// matches from a series of providers into one AutocompleteResult.
class AutocompleteController : public AutocompleteProviderListener {
 public:
  // Used to indicate an index that is not selected in a call to Update().
  static const int kNoItemSelected;

  // |provider_types| is a bitmap containing AutocompleteProvider::Type values
  // that will (potentially, depending on platform, flags, etc.) be
  // instantiated.
  AutocompleteController(Profile* profile,
                         AutocompleteControllerDelegate* delegate,
                         int provider_types);
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

  // Begin asynchronously fetching zero-suggest suggestions for |url|.
  // |user_text| is the text entered in the omnibox, which may be non-empty if
  // the user previously focused in the omnibox during this interaction.
  // TODO(jered): Rip out |user_text| once the first match is decoupled from
  // the current typing in the omnibox.
  void StartZeroSuggest(const GURL& url, const string16& user_text);

  // Cancels any pending zero-suggest fetch.
  void StopZeroSuggest();

  // Asks the relevant provider to delete |match|, and ensures observers are
  // notified of resulting changes immediately.  This should only be called when
  // no query is running.
  void DeleteMatch(const AutocompleteMatch& match);

  // Removes any entries that were copied from the last result. This is used by
  // the popup to ensure it's not showing an out-of-date query.
  void ExpireCopiedEntries();

  SearchProvider* search_provider() const { return search_provider_; }
  KeywordProvider* keyword_provider() const { return keyword_provider_; }

  const AutocompleteInput& input() const { return input_; }
  const AutocompleteResult& result() const { return result_; }
  bool done() const { return done_; }
  const ACProviders* providers() const { return &providers_; }

  // AutocompleteProviderListener:
  virtual void OnProviderUpdate(bool updated_matches) OVERRIDE;

  // Called when an omnibox event log entry is generated.
  // Populates provider_info with diagnostic information about the status
  // of various providers.  In turn, calls
  // AutocompleteProvider::AddProviderInfo() so each provider can add
  // provider-specific information, information we want to log for a particular
  // provider but not others.
  void AddProvidersInfo(ProvidersInfo* provider_info) const;

 private:
  friend class AutocompleteProviderTest;
  FRIEND_TEST_ALL_PREFIXES(AutocompleteProviderTest,
                           RedundantKeywordsIgnoredInResult);
  FRIEND_TEST_ALL_PREFIXES(AutocompleteProviderTest, UpdateAssistedQueryStats);

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

  // For each AutocompleteMatch returned by SearchProvider, updates the
  // destination_url iff the provider's TemplateURL supports assisted query
  // stats.
  void UpdateAssistedQueryStats(AutocompleteResult* result);

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

  ZeroSuggestProvider* zero_suggest_provider_;

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

  // Has StartZeroSuggest() been called but not Start()?
  bool in_zero_suggest_;

  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(AutocompleteController);
};

#endif  // CHROME_BROWSER_AUTOCOMPLETE_AUTOCOMPLETE_CONTROLLER_H_
