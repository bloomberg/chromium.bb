// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOCOMPLETE_HISTORY_URL_PROVIDER_H_
#define CHROME_BROWSER_AUTOCOMPLETE_HISTORY_URL_PROVIDER_H_
#pragma once

#include <string>

#include "base/compiler_specific.h"
#include "chrome/browser/autocomplete/history_provider.h"
#include "chrome/browser/autocomplete/history_provider_util.h"

class MessageLoop;
class Profile;

namespace history {

class HistoryBackend;
class URLDatabase;
class URLRow;

}  // namespace history

// How history autocomplete works
// ==============================
//
// Read down this diagram for temporal ordering.
//
//   Main thread                History thread
//   -----------                --------------
//   AutocompleteController::Start
//     -> HistoryURLProvider::Start
//       -> RunAutocompletePasses
//         -> SuggestExactInput
//         [params_ allocated]
//         -> DoAutocomplete (for inline autocomplete)
//           -> URLDatabase::AutocompleteForPrefix (on in-memory DB)
//         -> HistoryService::ScheduleAutocomplete
//         (return to controller) ----
//                                   /
//                              HistoryBackend::ScheduleAutocomplete
//                                -> HistoryURLProvider::ExecuteWithDB
//                                  -> DoAutocomplete
//                                    -> URLDatabase::AutocompleteForPrefix
//                                /
//   HistoryService::QueryComplete
//     [params_ destroyed]
//     -> AutocompleteProvider::Listener::OnProviderUpdate
//
// The autocomplete controller calls us, and must be called back, on the main
// thread.  When called, we run two autocomplete passes.  The first pass runs
// synchronously on the main thread and queries the in-memory URL database.
// This pass promotes matches for inline autocomplete if applicable.  We do
// this synchronously so that users get consistent behavior when they type
// quickly and hit enter, no matter how loaded the main history database is.
// Doing this synchronously also prevents inline autocomplete from being
// "flickery" in the AutocompleteEdit.  Because the in-memory DB does not have
// redirect data, results other than the top match might change between the
// two passes, so we can't just decide to use this pass' matches as the final
// results.
//
// The second autocomplete pass uses the full history database, which must be
// queried on the history thread.  Start() asks the history service schedule to
// callback on the history thread with a pointer to the main database.  When we
// are done doing queries, we schedule a task on the main thread that notifies
// the AutocompleteController that we're done.
//
// The communication between these threads is done using a
// HistoryURLProviderParams object.  This is allocated in the main thread, and
// normally deleted in QueryComplete().  So that both autocomplete passes can
// use the same code, we also use this to hold results during the first
// autocomplete pass.
//
// While the second pass is running, the AutocompleteController may cancel the
// request.  This can happen frequently when the user is typing quickly.  In
// this case, the main thread sets params_->cancel, which the background thread
// checks periodically.  If it finds the flag set, it stops what it's doing
// immediately and calls back to the main thread.  (We don't delete the params
// on the history thread, because we should only do that when we can safely
// NULL out params_, and that must be done on the main thread.)

// Used to communicate autocomplete parameters between threads via the history
// service.
struct HistoryURLProviderParams {
  HistoryURLProviderParams(const AutocompleteInput& input,
                           bool trim_http,
                           const std::string& languages);
  ~HistoryURLProviderParams();

  MessageLoop* message_loop;

  // A copy of the autocomplete input. We need the copy since this object will
  // live beyond the original query while it runs on the history thread.
  AutocompleteInput input;

  // Set when "http://" should be trimmed from the beginning of the URLs.
  bool trim_http;

  // Set by the main thread to cancel this request. READ ONLY when running in
  // ExecuteWithDB() on the history thread to prevent deadlock. If this flag is
  // set when the query runs, the query will be abandoned. This allows us to
  // avoid running queries that are no longer needed. Since we don't care if
  // we run the extra queries, the lack of signaling is not a problem.
  bool cancel;

  // Set by ExecuteWithDB() on the history thread when the query could not be
  // performed because the history system failed to properly init the database.
  // If this is set when the main thread is called back, it avoids changing
  // |matches_| at all, so it won't delete the default match
  // RunAutocompletePasses() creates.
  bool failed;

  // List of matches written by the history thread.  We keep this separate list
  // to avoid having the main thread read the provider's matches while the
  // history thread is manipulating them.  The provider copies this list back
  // to matches_ on the main thread in QueryComplete().
  ACMatches matches;

  // Languages we should pass to gfx::GetCleanStringFromUrl.
  std::string languages;

 private:
  DISALLOW_COPY_AND_ASSIGN(HistoryURLProviderParams);
};

// This class is an autocomplete provider and is also a pseudo-internal
// component of the history system.  See comments above.
//
// Note: This object can get leaked on shutdown if there are pending
// requests on the database (which hold a reference to us). Normally, these
// messages get flushed for each thread. We do a round trip from main, to
// history, back to main while holding a reference. If the main thread
// completes before the history thread, the message to delegate back to the
// main thread will not run and the reference will leak. Therefore, don't do
// anything on destruction.
class HistoryURLProvider : public HistoryProvider {
 public:
  HistoryURLProvider(ACProviderListener* listener, Profile* profile);

#ifdef UNIT_TEST
  HistoryURLProvider(ACProviderListener* listener,
                     Profile* profile,
                     const std::string& languages)
    : HistoryProvider(listener, profile, "History"),
      prefixes_(GetPrefixes()),
      params_(NULL),
      languages_(languages) {}
#endif
  // no destructor (see note above)

  // AutocompleteProvider
  virtual void Start(const AutocompleteInput& input,
                     bool minimal_changes) OVERRIDE;
  virtual void Stop() OVERRIDE;

  // Runs the history query on the history thread, called by the history
  // system. The history database MAY BE NULL in which case it is not
  // available and we should return no data. Also schedules returning the
  // results to the main thread
  void ExecuteWithDB(history::HistoryBackend* backend,
                     history::URLDatabase* db,
                     HistoryURLProviderParams* params);

  // Actually runs the autocomplete job on the given database, which is
  // guaranteed not to be NULL.
  void DoAutocomplete(history::HistoryBackend* backend,
                      history::URLDatabase* db,
                      HistoryURLProviderParams* params);

  // Dispatches the results to the autocomplete controller. Called on the
  // main thread by ExecuteWithDB when the results are available.
  // Frees params_gets_deleted on exit.
  void QueryComplete(HistoryURLProviderParams* params_gets_deleted);

 private:
  ~HistoryURLProvider() {}

  // Returns the set of prefixes to use for prefixes_.
  static history::Prefixes GetPrefixes();

  // Determines the relevance for some input, given its type and which match it
  // is.  If |match_type| is NORMAL, |match_number| is a number
  // [0, kMaxSuggestions) indicating the relevance of the match (higher == more
  // relevant).  For other values of |match_type|, |match_number| is ignored.
  static int CalculateRelevance(AutocompleteInput::Type input_type,
                                MatchType match_type,
                                size_t match_number);

  // Given the user's |input| and a |match| created from it, reduce the
  // match's URL to just a host.  If this host still matches the user input,
  // return it.  Returns the empty string on failure.
  static GURL ConvertToHostOnly(const history::HistoryMatch& match,
                                const std::wstring& input);

  // See if a shorter version of the best match should be created, and if so
  // place it at the front of |matches|.  This can suggest history URLs that
  // are prefixes of the best match (if they've been visited enough, compared
  // to the best match), or create host-only suggestions even when they haven't
  // been visited before: if the user visited http://example.com/asdf once,
  // we'll suggest http://example.com/ even if they've never been to it.  See
  // the function body for the exact heuristics used.
  static void PromoteOrCreateShorterSuggestion(
      history::URLDatabase* db,
      const HistoryURLProviderParams& params,
      bool have_what_you_typed_match,
      const AutocompleteMatch& what_you_typed_match,
      history::HistoryMatches* matches);

  // Ensures that |matches| contains an entry for |info|, which may mean adding
  // a new such entry (using |input_location| and |match_in_scheme|).
  //
  // If |promote| is true, this also ensures the entry is the first element in
  // |matches|, moving or adding it to the front as appropriate.  When
  // |promote| is false, existing matches are left in place, and newly added
  // matches are placed at the back.
  static void EnsureMatchPresent(const history::URLRow& info,
                                 std::wstring::size_type input_location,
                                 bool match_in_scheme,
                                 history::HistoryMatches* matches,
                                 bool promote);

  // Helper function that actually launches the two autocomplete passes.
  void RunAutocompletePasses(const AutocompleteInput& input,
                             bool fixup_input_and_run_pass_1);

  // Returns the best prefix that begins |text|.  "Best" means "greatest number
  // of components".  This may return NULL if no prefix begins |text|.
  //
  // |prefix_suffix| (which may be empty) is appended to every attempted
  // prefix.  This is useful when you need to figure out the innermost match
  // for some user input in a URL.
  const history::Prefix* BestPrefix(const GURL& text,
                                    const std::wstring& prefix_suffix) const;

  // Returns a match corresponding to exactly what the user has typed.
  AutocompleteMatch SuggestExactInput(const AutocompleteInput& input,
                                      bool trim_http);

  // Given a |match| containing the "what you typed" suggestion created by
  // SuggestExactInput(), looks up its info in the DB.  If found, fills in the
  // title from the DB, promotes the match's priority to that of an inline
  // autocomplete match (maybe it should be slightly better?), and places it on
  // the front of |matches| (so we pick the right matches to throw away
  // when culling redirects to/from it).  Returns whether a match was promoted.
  bool FixupExactSuggestion(history::URLDatabase* db,
                            const AutocompleteInput& input,
                            AutocompleteMatch* match,
                            history::HistoryMatches* matches) const;

  // Determines if |match| is suitable for inline autocomplete, and promotes it
  // if so.
  bool PromoteMatchForInlineAutocomplete(HistoryURLProviderParams* params,
                                         const history::HistoryMatch& match);

  // Sorts the given list of matches.
  void SortMatches(history::HistoryMatches* matches) const;

  // Removes results that have been rarely typed or visited, and not any time
  // recently.  The exact parameters for this heuristic can be found in the
  // function body.
  void CullPoorMatches(history::HistoryMatches* matches) const;

  // Removes results that redirect to each other, leaving at most |max_results|
  // results.
  void CullRedirects(history::HistoryBackend* backend,
                     history::HistoryMatches* matches,
                     size_t max_results) const;

  // Helper function for CullRedirects, this removes all but the first
  // occurance of [any of the set of strings in |remove|] from the |matches|
  // list.
  //
  // The return value is the index of the item that is after the item in the
  // input identified by |source_index|. If |source_index| or an item before
  // is removed, the next item will be shifted, and this allows the caller to
  // pick up on the next one when this happens.
  size_t RemoveSubsequentMatchesOf(history::HistoryMatches* matches,
                                   size_t source_index,
                                   const std::vector<GURL>& remove) const;

  // Converts a line from the database into an autocomplete match for display.
  AutocompleteMatch HistoryMatchToACMatch(
      HistoryURLProviderParams* params,
      const history::HistoryMatch& history_match,
      MatchType match_type,
      size_t match_number);

  // Prefixes to try appending to user input when looking for a match.
  const history::Prefixes prefixes_;

  // Params for the current query.  The provider should not free this directly;
  // instead, it is passed as a parameter through the history backend, and the
  // parameter itself is freed once it's no longer needed.  The only reason we
  // keep this member is so we can set the cancel bit on it.
  HistoryURLProviderParams* params_;

  // Only used by unittests; if non-empty, overrides accept-languages in the
  // profile's pref system.
  std::string languages_;
};

#endif  // CHROME_BROWSER_AUTOCOMPLETE_HISTORY_URL_PROVIDER_H_
