// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOCOMPLETE_AUTOCOMPLETE_CONTROLLER_H_
#define CHROME_BROWSER_AUTOCOMPLETE_AUTOCOMPLETE_CONTROLLER_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/string16.h"
#include "base/time.h"
#include "base/timer.h"
#include "chrome/browser/autocomplete/autocomplete_input.h"
#include "chrome/browser/autocomplete/autocomplete_provider.h"
#include "chrome/browser/autocomplete/autocomplete_provider_listener.h"
#include "chrome/browser/autocomplete/autocomplete_result.h"

class AutocompleteControllerDelegate;
class HistoryURLProvider;
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
  // See AutocompleteInput::AutocompleteInput(...) for more details regarding
  // |input| params.
  //
  // The controller calls AutocompleteControllerDelegate::OnResultChanged() from
  // inside this call at least once. If matches are available later on that
  // result in changing the result set the delegate is notified again. When the
  // controller is done the notification AUTOCOMPLETE_CONTROLLER_RESULT_READY is
  // sent.
  void Start(const AutocompleteInput& input);

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

  // AutocompleteProviderListener:
  virtual void OnProviderUpdate(bool updated_matches) OVERRIDE;

  // Called when an omnibox event log entry is generated.
  // Populates provider_info with diagnostic information about the status
  // of various providers.  In turn, calls
  // AutocompleteProvider::AddProviderInfo() so each provider can add
  // provider-specific information, information we want to log for a particular
  // provider but not others.
  void AddProvidersInfo(ProvidersInfo* provider_info) const;

  // Called when a new omnibox session starts.
  // We start a new session when the user first begins modifying the omnibox
  // content; see |OmniboxEditModel::user_input_in_progress_|.
  void ResetSession();

  // Constructs the final destination URL for a given match using additional
  // parameters otherwise not available at initial construction time.  This
  // method should be called from OmniboxEditModel::OpenMatch() before the user
  // navigates to the selected match.
  GURL GetDestinationURL(const AutocompleteMatch& match,
                         base::TimeDelta query_formulation_time) const;

  HistoryURLProvider* history_url_provider() const {
    return history_url_provider_;
  }
  KeywordProvider* keyword_provider() const { return keyword_provider_; }
  SearchProvider* search_provider() const { return search_provider_; }

  const AutocompleteInput& input() const { return input_; }
  const AutocompleteResult& result() const { return result_; }
  bool done() const { return done_; }
  const ACProviders* providers() const { return &providers_; }

  const base::TimeTicks& last_time_default_match_changed() const {
    return last_time_default_match_changed_;
  }

 private:
  friend class AutocompleteProviderTest;
  FRIEND_TEST_ALL_PREFIXES(AutocompleteProviderTest,
                           RedundantKeywordsIgnoredInResult);
  FRIEND_TEST_ALL_PREFIXES(AutocompleteProviderTest, UpdateAssistedQueryStats);
  FRIEND_TEST_ALL_PREFIXES(AutocompleteProviderTest, GetDestinationURL);

  // Updates |result_| to reflect the current provider state and fires
  // notifications.  If |regenerate_result| then we clear the result
  // so when we incorporate the current provider state we end up
  // implicitly removing all expired matches.  (Normally we allow
  // matches from the previous result set carry over.  These stale
  // results may outrank legitimate matches from the current result
  // set.  Sometimes we just want the current matches; the easier way
  // to do this is to throw everything out and reconstruct the result
  // set from the providers' current data.)
  // If |force_notify_default_match_changed|, we tell NotifyChanged
  // the default match has changed even if it hasn't.  This is
  // necessary in some cases; for instance, if the user typed a new
  // character, the edit model needs to repaint (highlighting changed)
  // even if the default match didn't change.
  void UpdateResult(bool regenerate_result,
                    bool force_notify_default_match_changed);

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

  HistoryURLProvider* history_url_provider_;

  KeywordProvider* keyword_provider_;

  SearchProvider* search_provider_;

  ZeroSuggestProvider* zero_suggest_provider_;

  // Input passed to Start.
  AutocompleteInput input_;

  // Data from the autocomplete query.
  AutocompleteResult result_;

  // The most recent time the default match (inline match) changed.  This may
  // be earlier than the most recent keystroke if the recent keystrokes didn't
  // change the suggested match in the omnibox.  (For instance, if
  // a user typed "mail.goog" and the match https://mail.google.com/ was
  // the destination match ever since the user typed "ma" then this is
  // the time that URL first appeared as the default match.)  This may
  // also be more recent than the last keystroke if there was an
  // asynchronous provider that returned and changed the default
  // match.  See UpdateResult() for details on when we consider a
  // match to have changed.
  base::TimeTicks last_time_default_match_changed_;

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
