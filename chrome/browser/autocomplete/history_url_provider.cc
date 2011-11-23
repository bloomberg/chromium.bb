// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autocomplete/history_url_provider.h"

#include <algorithm>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autocomplete/autocomplete_match.h"
#include "chrome/browser/history/history.h"
#include "chrome/browser/history/history_backend.h"
#include "chrome/browser/history/history_database.h"
#include "chrome/browser/history/history_types.h"
#include "chrome/browser/net/url_fixer_upper.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "googleurl/src/gurl.h"
#include "googleurl/src/url_parse.h"
#include "googleurl/src/url_util.h"
#include "net/base/net_util.h"
#include "net/base/registry_controlled_domain.h"

namespace {

// Ensures that |matches| contains an entry for |info|, which may mean adding a
// new such entry (using |input_location| and |match_in_scheme|).
//
// If |promote| is true, this also ensures the entry is the first element in
// |matches|, moving or adding it to the front as appropriate.  When |promote|
// is false, existing matches are left in place, and newly added matches are
// placed at the back.
void EnsureMatchPresent(const history::URLRow& info,
                        size_t input_location,
                        bool match_in_scheme,
                        history::HistoryMatches* matches,
                        bool promote) {
  // |matches| may already have an entry for this.
  for (history::HistoryMatches::iterator i(matches->begin());
       i != matches->end(); ++i) {
    if (i->url_info.url() == info.url()) {
      // Rotate it to the front if the caller wishes.
      if (promote)
        std::rotate(matches->begin(), i, i + 1);
      return;
    }
  }

  // No entry, so create one.
  history::HistoryMatch match(info, input_location, match_in_scheme, true);
  if (promote)
    matches->push_front(match);
  else
    matches->push_back(match);
}

// Given the user's |input| and a |match| created from it, reduce the match's
// URL to just a host.  If this host still matches the user input, return it.
// Returns the empty string on failure.
GURL ConvertToHostOnly(const history::HistoryMatch& match,
                       const string16& input) {
  // See if we should try to do host-only suggestions for this URL. Nonstandard
  // schemes means there's no authority section, so suggesting the host name
  // is useless. File URLs are standard, but host suggestion is not useful for
  // them either.
  const GURL& url = match.url_info.url();
  if (!url.is_valid() || !url.IsStandard() || url.SchemeIsFile())
    return GURL();

  // Transform to a host-only match.  Bail if the host no longer matches the
  // user input (e.g. because the user typed more than just a host).
  GURL host = url.GetWithEmptyPath();
  if ((host.spec().length() < (match.input_location + input.length())))
    return GURL();  // User typing is longer than this host suggestion.

  const string16 spec = UTF8ToUTF16(host.spec());
  if (spec.compare(match.input_location, input.length(), input))
    return GURL();  // User typing is no longer a prefix.

  return host;
}

// See if a shorter version of the best match should be created, and if so place
// it at the front of |matches|.  This can suggest history URLs that are
// prefixes of the best match (if they've been visited enough, compared to the
// best match), or create host-only suggestions even when they haven't been
// visited before: if the user visited http://example.com/asdf once, we'll
// suggest http://example.com/ even if they've never been to it.
void PromoteOrCreateShorterSuggestion(
    history::URLDatabase* db,
    const HistoryURLProviderParams& params,
    bool have_what_you_typed_match,
    const AutocompleteMatch& what_you_typed_match,
    history::HistoryMatches* matches) {
  if (matches->empty())
    return;  // No matches, nothing to do.

  // Determine the base URL from which to search, and whether that URL could
  // itself be added as a match.  We can add the base iff it's not "effectively
  // the same" as any "what you typed" match.
  const history::HistoryMatch& match = matches->front();
  GURL search_base = ConvertToHostOnly(match, params.input.text());
  bool can_add_search_base_to_matches = !have_what_you_typed_match;
  if (search_base.is_empty()) {
    // Search from what the user typed when we couldn't reduce the best match
    // to a host.  Careful: use a substring of |match| here, rather than the
    // first match in |params|, because they might have different prefixes.  If
    // the user typed "google.com", |what_you_typed_match| will hold
    // "http://google.com/", but |match| might begin with
    // "http://www.google.com/".
    // TODO: this should be cleaned up, and is probably incorrect for IDN.
    std::string new_match = match.url_info.url().possibly_invalid_spec().
        substr(0, match.input_location + params.input.text().length());
    search_base = GURL(new_match);
    // TODO(mrossetti): There is a degenerate case where the following may
    // cause a failure: http://www/~someword/fubar.html. Diagnose.
    // See: http://crbug.com/50101
    if (search_base.is_empty())
      return;  // Can't construct a valid URL from which to start a search.
  } else if (!can_add_search_base_to_matches) {
    can_add_search_base_to_matches =
        (search_base != what_you_typed_match.destination_url);
  }
  if (search_base == match.url_info.url())
    return;  // Couldn't shorten |match|, so no range of URLs to search over.

  // Search the DB for short URLs between our base and |match|.
  history::URLRow info(search_base);
  bool promote = true;
  // A short URL is only worth suggesting if it's been visited at least a third
  // as often as the longer URL.
  const int min_visit_count = ((match.url_info.visit_count() - 1) / 3) + 1;
  // For stability between the in-memory and on-disk autocomplete passes, when
  // the long URL has been typed before, only suggest shorter URLs that have
  // also been typed.  Otherwise, the on-disk pass could suggest a shorter URL
  // (which hasn't been typed) that the in-memory pass doesn't know about,
  // thereby making the top match, and thus the behavior of inline
  // autocomplete, unstable.
  const int min_typed_count = match.url_info.typed_count() ? 1 : 0;
  if (!db->FindShortestURLFromBase(search_base.possibly_invalid_spec(),
          match.url_info.url().possibly_invalid_spec(), min_visit_count,
          min_typed_count, can_add_search_base_to_matches, &info)) {
    if (!can_add_search_base_to_matches)
      return;  // Couldn't find anything and can't add the search base, bail.

    // Try to get info on the search base itself.  Promote it to the top if the
    // original best match isn't good enough to autocomplete.
    db->GetRowForURL(search_base, &info);
    promote = match.url_info.typed_count() <= 1;
  }

  // Promote or add the desired URL to the list of matches.
  EnsureMatchPresent(info, match.input_location, match.match_in_scheme,
                     matches, promote);
}

// Returns true if |url| is just a host (e.g. "http://www.google.com/") and not
// some other subpage (e.g. "http://www.google.com/foo.html").
bool IsHostOnly(const GURL& url) {
  DCHECK(url.is_valid());
  return (!url.has_path() || (url.path() == "/")) && !url.has_query() &&
      !url.has_ref();
}

// Acts like the > operator for URLInfo classes.
bool CompareHistoryMatch(const history::HistoryMatch& a,
                         const history::HistoryMatch& b) {
  // A URL that has been typed at all is better than one that has never been
  // typed.  (Note "!"s on each side)
  if (!a.url_info.typed_count() != !b.url_info.typed_count())
    return a.url_info.typed_count() > b.url_info.typed_count();

  // Innermost matches (matches after any scheme or "www.") are better than
  // non-innermost matches.
  if (a.innermost_match != b.innermost_match)
    return a.innermost_match;

  // URLs that have been typed more often are better.
  if (a.url_info.typed_count() != b.url_info.typed_count())
    return a.url_info.typed_count() > b.url_info.typed_count();

  // For URLs that have each been typed once, a host (alone) is better than a
  // page inside.
  if (a.url_info.typed_count() == 1) {
    const bool a_is_host_only = IsHostOnly(a.url_info.url());
    if (a_is_host_only != IsHostOnly(b.url_info.url()))
      return a_is_host_only;
  }

  // URLs that have been visited more often are better.
  if (a.url_info.visit_count() != b.url_info.visit_count())
    return a.url_info.visit_count() > b.url_info.visit_count();

  // URLs that have been visited more recently are better.
  return a.url_info.last_visit() > b.url_info.last_visit();
}

}  // namespace

// VisitClassifier is used to classify the type of visit to a particular url.
class HistoryURLProvider::VisitClassifier {
 public:
  enum Type {
    INVALID,             // Navigations to the URL are not allowed.
    UNVISITED,           // A navigable URL for which we have no visit data.
    UNVISITED_INTRANET,  // A URL which is UNVISITED but which is known to
                         // refer to a visited intranet host.
    VISITED,             // The site has been previously visited.
  };

  VisitClassifier(HistoryURLProvider* provider,
                  const AutocompleteInput& input,
                  history::URLDatabase* db);

  // Returns the type of visit for the specified input.
  Type type() const { return type_; }

  // Returns the URLRow for the visit.
  const history::URLRow& url_row() const { return url_row_; }

 private:
  HistoryURLProvider* provider_;
  history::URLDatabase* db_;
  Type type_;
  history::URLRow url_row_;

  DISALLOW_COPY_AND_ASSIGN(VisitClassifier);
};

HistoryURLProvider::VisitClassifier::VisitClassifier(
    HistoryURLProvider* provider,
    const AutocompleteInput& input,
    history::URLDatabase* db)
    : provider_(provider),
      db_(db),
      type_(INVALID) {
  const GURL& url = input.canonicalized_url();
  // Detect email addresses.  These cases will look like "http://user@site/",
  // and because the history backend strips auth creds, we'll get a bogus exact
  // match below if the user has visited "site".
  if (!url.is_valid() ||
      ((input.type() == AutocompleteInput::UNKNOWN) &&
       input.parts().username.is_nonempty() &&
       !input.parts().password.is_nonempty() &&
       !input.parts().path.is_nonempty()))
    return;

  if (db_->GetRowForURL(url, &url_row_)) {
    type_ = VISITED;
    return;
  }

  if (provider_->CanFindIntranetURL(db_, input)) {
    // The user typed an intranet hostname that they've visited (albeit with a
    // different port and/or path) before.
    url_row_ = history::URLRow(url);
    type_ = UNVISITED_INTRANET;
    return;
  }

  // Tricky corner case: The user has visited intranet site "foo", but not
  // internet site "www.foo.com".  He types in foo (getting an exact match),
  // then tries to hit ctrl-enter.  When pressing ctrl, the what-you-typed match
  // ("www.foo.com") doesn't show up in history, and thus doesn't get a promoted
  // relevance, but a different match from the input ("foo") does, and gets
  // promoted for inline autocomplete.  Thus instead of getting "www.foo.com",
  // the user still gets "foo" (and, before hitting enter, probably gets an
  // odd-looking inline autocomplete of "/").
  //
  // We detect this crazy case as follows:
  // * If the what-you-typed match is not in the history DB,
  // * and the user has specified a TLD,
  // * and the input _without_ the TLD _is_ in the history DB,
  // * ...then just before pressing "ctrl" the best match we supplied was the
  //   what-you-typed match, so stick with it by promoting this.
  if (input.desired_tld().empty())
    return;
  GURL destination_url(URLFixerUpper::FixupURL(UTF16ToUTF8(input.text()),
                                               std::string()));
  if (!db_->GetRowForURL(destination_url, NULL))
    return;
  // If we got here, then we hit the tricky corner case.
  url_row_ = history::URLRow(url);
  type_ = UNVISITED;
}

HistoryURLProviderParams::HistoryURLProviderParams(
    const AutocompleteInput& input,
    bool trim_http,
    const std::string& languages)
    : message_loop(MessageLoop::current()),
      input(input),
      prevent_inline_autocomplete(input.prevent_inline_autocomplete()),
      trim_http(trim_http),
      failed(false),
      languages(languages),
      dont_suggest_exact_input(false) {
}

HistoryURLProviderParams::~HistoryURLProviderParams() {
}

HistoryURLProvider::HistoryURLProvider(ACProviderListener* listener,
                                       Profile* profile)
    : HistoryProvider(listener, profile, "HistoryURL"),
      prefixes_(GetPrefixes()),
      params_(NULL) {
}

void HistoryURLProvider::Start(const AutocompleteInput& input,
                               bool minimal_changes) {
  // NOTE: We could try hard to do less work in the |minimal_changes| case
  // here; some clever caching would let us reuse the raw matches from the
  // history DB without re-querying.  However, we'd still have to go back to
  // the history thread to mark these up properly, and if pass 2 is currently
  // running, we'd need to wait for it to return to the main thread before
  // doing this (we can't just write new data for it to read due to thread
  // safety issues).  At that point it's just as fast, and easier, to simply
  // re-run the query from scratch and ignore |minimal_changes|.

  // Cancel any in-progress query.
  Stop();

  RunAutocompletePasses(input, true);
}

void HistoryURLProvider::Stop() {
  done_ = true;

  if (params_)
    params_->cancel_flag.Set();
}

// Called on the history thread.
void HistoryURLProvider::ExecuteWithDB(history::HistoryBackend* backend,
                                       history::URLDatabase* db,
                                       HistoryURLProviderParams* params) {
  // We may get called with a NULL database if it couldn't be properly
  // initialized.
  if (!db) {
    params->failed = true;
  } else if (!params->cancel_flag.IsSet()) {
    base::TimeTicks beginning_time = base::TimeTicks::Now();

    DoAutocomplete(backend, db, params);

    UMA_HISTOGRAM_TIMES("Autocomplete.HistoryAsyncQueryTime",
                        base::TimeTicks::Now() - beginning_time);
  }

  // Return the results (if any) to the main thread.
  params->message_loop->PostTask(FROM_HERE, base::Bind(
      &HistoryURLProvider::QueryComplete, this, params));
}

// Used by both autocomplete passes, and therefore called on multiple different
// threads (though not simultaneously).
void HistoryURLProvider::DoAutocomplete(history::HistoryBackend* backend,
                                        history::URLDatabase* db,
                                        HistoryURLProviderParams* params) {
  VisitClassifier classifier(this, params->input, db);
  // Create a What You Typed match, which we'll need below.
  //
  // We display this to the user when there's a reasonable chance they actually
  // care:
  // * Their input can be opened as a URL, and
  // * They hit ctrl-enter, or we parsed the input as a URL, or it starts with
  //   an explicit "http:" or "https:".
  // Otherwise, this is just low-quality noise.  In the cases where we've parsed
  // as UNKNOWN, we'll still show an accidental search infobar if need be.
  bool have_what_you_typed_match =
      params->input.canonicalized_url().is_valid() &&
      (params->input.type() != AutocompleteInput::QUERY) &&
      ((params->input.type() != AutocompleteInput::UNKNOWN) ||
       (classifier.type() == VisitClassifier::UNVISITED_INTRANET) ||
       !params->trim_http ||
       (AutocompleteInput::NumNonHostComponents(params->input.parts()) > 0));
  AutocompleteMatch what_you_typed_match(SuggestExactInput(params->input,
                                                           params->trim_http));

  // Get the matching URLs from the DB
  typedef std::vector<history::URLRow> URLRowVector;
  URLRowVector url_matches;
  history::HistoryMatches history_matches;

  for (history::Prefixes::const_iterator i(prefixes_.begin());
       i != prefixes_.end(); ++i) {
    if (params->cancel_flag.IsSet())
      return;  // Canceled in the middle of a query, give up.
    // We only need kMaxMatches results in the end, but before we get there we
    // need to promote lower-quality matches that are prefixes of
    // higher-quality matches, and remove lower-quality redirects.  So we ask
    // for more results than we need, of every prefix type, in hopes this will
    // give us far more than enough to work with.  CullRedirects() will then
    // reduce the list to the best kMaxMatches results.
    db->AutocompleteForPrefix(UTF16ToUTF8(i->prefix + params->input.text()),
                              kMaxMatches * 2, (backend == NULL), &url_matches);
    for (URLRowVector::const_iterator j(url_matches.begin());
         j != url_matches.end(); ++j) {
      const history::Prefix* best_prefix = BestPrefix(j->url(), string16());
      DCHECK(best_prefix != NULL);
      history_matches.push_back(history::HistoryMatch(*j, i->prefix.length(),
          !i->num_components,
          i->num_components >= best_prefix->num_components));
    }
  }

  // Create sorted list of suggestions.
  CullPoorMatches(&history_matches);
  SortMatches(&history_matches);
  PromoteOrCreateShorterSuggestion(db, *params, have_what_you_typed_match,
                                   what_you_typed_match, &history_matches);

  // Try to promote a match as an exact/inline autocomplete match.  This also
  // moves it to the front of |history_matches|, so skip over it when
  // converting the rest of the matches.
  size_t first_match = 1;
  size_t exact_suggestion = 0;
  // Checking |is_history_what_you_typed_match| tells us whether
  // SuggestExactInput() succeeded in constructing a valid match.
  if (what_you_typed_match.is_history_what_you_typed_match &&
      (!backend || !params->dont_suggest_exact_input) &&
      FixupExactSuggestion(db, params->input, classifier, &what_you_typed_match,
                           &history_matches)) {
    // Got an exact match for the user's input.  Treat it as the best match
    // regardless of the input type.
    exact_suggestion = 1;
    params->matches.push_back(what_you_typed_match);
  } else if (params->prevent_inline_autocomplete ||
      history_matches.empty() ||
      !PromoteMatchForInlineAutocomplete(params, history_matches.front(),
                                         history_matches)) {
    // Failed to promote any URLs for inline autocompletion.  Use the What You
    // Typed match, if we have it.
    first_match = 0;
    if (have_what_you_typed_match)
      params->matches.push_back(what_you_typed_match);
  }

  // This is the end of the synchronous pass.
  if (!backend)
    return;

  // Remove redirects and trim list to size.  We want to provide up to
  // kMaxMatches results plus the What You Typed result, if it was added to
  // |history_matches| above.
  CullRedirects(backend, &history_matches, kMaxMatches + exact_suggestion);

  // Convert the history matches to autocomplete matches.
  for (size_t i = first_match; i < history_matches.size(); ++i) {
    const history::HistoryMatch& match = history_matches[i];
    DCHECK(!have_what_you_typed_match ||
           (match.url_info.url() !=
            GURL(params->matches.front().destination_url)));
    AutocompleteMatch ac_match =
        HistoryMatchToACMatch(params, match, history_matches, NORMAL,
                              history_matches.size() - 1 - i);
    params->matches.push_back(ac_match);
  }
}

// Called on the main thread when the query is complete.
void HistoryURLProvider::QueryComplete(
    HistoryURLProviderParams* params_gets_deleted) {
  // Ensure |params_gets_deleted| gets deleted on exit.
  scoped_ptr<HistoryURLProviderParams> params(params_gets_deleted);

  // If the user hasn't already started another query, clear our member pointer
  // so we can't write into deleted memory.
  if (params_ == params_gets_deleted)
    params_ = NULL;

  // Don't send responses for queries that have been canceled.
  if (params->cancel_flag.IsSet())
    return;  // Already set done_ when we canceled, no need to set it again.

  // Don't modify |matches_| if the query failed, since it might have a default
  // match in it, whereas |params->matches| will be empty.
  if (!params->failed) {
    matches_.swap(params->matches);
    UpdateStarredStateOfMatches();
  }

  done_ = true;
  listener_->OnProviderUpdate(true);
}

HistoryURLProvider::~HistoryURLProvider() {
  // Note: This object can get leaked on shutdown if there are pending
  // requests on the database (which hold a reference to us). Normally, these
  // messages get flushed for each thread. We do a round trip from main, to
  // history, back to main while holding a reference. If the main thread
  // completes before the history thread, the message to delegate back to the
  // main thread will not run and the reference will leak. Therefore, don't do
  // anything on destruction.
}

// static
history::Prefixes HistoryURLProvider::GetPrefixes() {
  // We'll complete text following these prefixes.
  // NOTE: There's no requirement that these be in any particular order.
  history::Prefixes prefixes;
  prefixes.push_back(history::Prefix(ASCIIToUTF16("https://www."), 2));
  prefixes.push_back(history::Prefix(ASCIIToUTF16("http://www."), 2));
  prefixes.push_back(history::Prefix(ASCIIToUTF16("ftp://ftp."), 2));
  prefixes.push_back(history::Prefix(ASCIIToUTF16("ftp://www."), 2));
  prefixes.push_back(history::Prefix(ASCIIToUTF16("https://"), 1));
  prefixes.push_back(history::Prefix(ASCIIToUTF16("http://"), 1));
  prefixes.push_back(history::Prefix(ASCIIToUTF16("ftp://"), 1));
  // Empty string catches within-scheme matches as well.
  prefixes.push_back(history::Prefix(string16(), 0));
  return prefixes;
}

// static
int HistoryURLProvider::CalculateRelevance(AutocompleteInput::Type input_type,
                                           MatchType match_type,
                                           size_t match_number) {
  switch (match_type) {
    case INLINE_AUTOCOMPLETE:
      return 1410;

    case UNVISITED_INTRANET:
      return 1400;

    case WHAT_YOU_TYPED:
      return 1200;

    default:
      return 900 + static_cast<int>(match_number);
  }
}

void HistoryURLProvider::RunAutocompletePasses(
    const AutocompleteInput& input,
    bool fixup_input_and_run_pass_1) {
  matches_.clear();

  if ((input.type() == AutocompleteInput::INVALID) ||
      (input.type() == AutocompleteInput::FORCED_QUERY))
    return;

  // Create a match for exactly what the user typed.  This will only be used as
  // a fallback in case we can't get the history service or URL DB; otherwise,
  // we'll run this again in DoAutocomplete() and use that result instead.
  const bool trim_http = !HasHTTPScheme(input.text());
  // Don't do this for queries -- while we can sometimes mark up a match for
  // this, it's not what the user wants, and just adds noise.
  if ((input.type() != AutocompleteInput::QUERY) &&
      input.canonicalized_url().is_valid())
    matches_.push_back(SuggestExactInput(input, trim_http));

  // We'll need the history service to run both passes, so try to obtain it.
  if (!profile_)
    return;
  HistoryService* const history_service =
      profile_->GetHistoryService(Profile::EXPLICIT_ACCESS);
  if (!history_service)
    return;

  // Create the data structure for the autocomplete passes.  We'll save this off
  // onto the |params_| member for later deletion below if we need to run pass
  // 2.
  std::string languages(languages_);
  if (languages.empty()) {
    languages =
        profile_->GetPrefs()->GetString(prefs::kAcceptLanguages);
  }
  scoped_ptr<HistoryURLProviderParams> params(
      new HistoryURLProviderParams(input, trim_http, languages));

  params->prevent_inline_autocomplete =
      PreventInlineAutocomplete(input);

  if (fixup_input_and_run_pass_1) {
    // Do some fixup on the user input before matching against it, so we provide
    // good results for local file paths, input with spaces, etc.
    if (!FixupUserInput(&params->input))
      return;

    // Pass 1: Get the in-memory URL database, and use it to find and promote
    // the inline autocomplete match, if any.
    history::URLDatabase* url_db = history_service->InMemoryDatabase();
    // url_db can be NULL if it hasn't finished initializing (or failed to
    // initialize).  In this case all we can do is fall back on the second
    // pass.
    //
    // TODO(pkasting): We should just block here until this loads.  Any time
    // someone unloads the history backend, we'll get inconsistent inline
    // autocomplete behavior here.
    if (url_db) {
      DoAutocomplete(NULL, url_db, params.get());
      // params->matches now has the matches we should expose to the provider.
      // Pass 2 expects a "clean slate" set of matches.
      matches_.clear();
      matches_.swap(params->matches);
      UpdateStarredStateOfMatches();
    }
  }

  // Pass 2: Ask the history service to call us back on the history thread,
  // where we can read the full on-disk DB.
  if (input.matches_requested() == AutocompleteInput::ALL_MATCHES) {
    done_ = false;
    params_ = params.release();  // This object will be destroyed in
                                 // QueryComplete() once we're done with it.
    history_service->ScheduleAutocomplete(this, params_);
  }
}

const history::Prefix* HistoryURLProvider::BestPrefix(
    const GURL& url,
    const string16& prefix_suffix) const {
  const history::Prefix* best_prefix = NULL;
  const string16 text(UTF8ToUTF16(url.spec()));
  for (history::Prefixes::const_iterator i(prefixes_.begin());
       i != prefixes_.end(); ++i) {
    if ((best_prefix == NULL) ||
        (i->num_components > best_prefix->num_components)) {
      string16 prefix_with_suffix(i->prefix + prefix_suffix);
      if ((text.length() >= prefix_with_suffix.length()) &&
          !text.compare(0, prefix_with_suffix.length(), prefix_with_suffix))
        best_prefix = &(*i);
    }
  }
  return best_prefix;
}

AutocompleteMatch HistoryURLProvider::SuggestExactInput(
    const AutocompleteInput& input,
    bool trim_http) {
  AutocompleteMatch match(this,
      CalculateRelevance(input.type(), WHAT_YOU_TYPED, 0), false,
      AutocompleteMatch::URL_WHAT_YOU_TYPED);

  const GURL& url = input.canonicalized_url();
  if (url.is_valid()) {
    match.destination_url = url;

    // Trim off "http://" if the user didn't type it.
    // NOTE: We use TrimHttpPrefix() here rather than StringForURLDisplay() to
    // strip the scheme as we need to know the offset so we can adjust the
    // |match_location| below.  StringForURLDisplay() and TrimHttpPrefix() have
    // slightly different behavior as well (the latter will strip even without
    // two slashes after the scheme).
    string16 display_string(StringForURLDisplay(url, false, false));
    const size_t offset = trim_http ? TrimHttpPrefix(&display_string) : 0;
    match.fill_into_edit =
        AutocompleteInput::FormattedStringWithEquivalentMeaning(url,
                                                                display_string);
    // NOTE: Don't set match.input_location (to allow inline autocompletion)
    // here, it's surprising and annoying.

    // Try to highlight "innermost" match location.  If we fix up "w" into
    // "www.w.com", we want to highlight the fifth character, not the first.
    // This relies on match.destination_url being the non-prefix-trimmed version
    // of match.contents.
    match.contents = display_string;
    const history::Prefix* best_prefix =
        BestPrefix(match.destination_url, input.text());
    // Because of the vagaries of GURL, it's possible for match.destination_url
    // to not contain the user's input at all.  In this case don't mark anything
    // as a match.
    const size_t match_location = (best_prefix == NULL) ?
        string16::npos : best_prefix->prefix.length() - offset;
    AutocompleteMatch::ClassifyLocationInString(match_location,
                                                input.text().length(),
                                                match.contents.length(),
                                                ACMatchClassification::URL,
                                                &match.contents_class);

    match.is_history_what_you_typed_match = true;
  }

  return match;
}

bool HistoryURLProvider::FixupExactSuggestion(
    history::URLDatabase* db,
    const AutocompleteInput& input,
    const VisitClassifier& classifier,
    AutocompleteMatch* match,
    history::HistoryMatches* matches) const {
  DCHECK(match != NULL);
  DCHECK(matches != NULL);

  MatchType type = INLINE_AUTOCOMPLETE;
  switch (classifier.type()) {
    case VisitClassifier::INVALID:
      return false;
    case VisitClassifier::VISITED:
      // We have data for this match, use it.
      match->deletable = true;
      match->description = classifier.url_row().title();
      AutocompleteMatch::ClassifyMatchInString(
          input.text(),
          classifier.url_row().title(),
          ACMatchClassification::NONE, &match->description_class);
      if (!classifier.url_row().typed_count()) {
        // If we reach here, we must be in the second pass, and we must not have
        // this row's data available during the first pass.  That means we
        // either scored it as WHAT_YOU_TYPED or UNVISITED_INTRANET, and to
        // maintain the ordering between passes consistent, we need to score it
        // the same way here.
        type = CanFindIntranetURL(db, input) ?
            UNVISITED_INTRANET : WHAT_YOU_TYPED;
      }
      break;
    case VisitClassifier::UNVISITED_INTRANET:
      type = UNVISITED_INTRANET;
      break;
    default:
      DCHECK_EQ(VisitClassifier::UNVISITED, classifier.type());
      break;
  }

  match->relevance = CalculateRelevance(input.type(), type, 0);

  if (type == UNVISITED_INTRANET && !matches->empty()) {
    // If there are any other matches, then don't promote this match here, in
    // hopes the caller will be able to inline autocomplete a better suggestion.
    // DoAutocomplete() will fall back on this match if inline autocompletion
    // fails.  This matches how we react to never-visited URL inputs in the non-
    // intranet case.
    return false;
  }

  // Put it on the front of the HistoryMatches for redirect culling.
  EnsureMatchPresent(classifier.url_row(), string16::npos, false, matches,
                     true);
  return true;
}

bool HistoryURLProvider::CanFindIntranetURL(
    history::URLDatabase* db,
    const AutocompleteInput& input) const {
  // Normally passing the first two conditions below ought to guarantee the
  // third condition, but because FixupUserInput() can run and modify the
  // input's text and parts between Parse() and here, it seems better to be
  // paranoid and check.
  if ((input.type() != AutocompleteInput::UNKNOWN) ||
      !LowerCaseEqualsASCII(input.scheme(), chrome::kHttpScheme) ||
      !input.parts().host.is_nonempty())
    return false;
  const std::string host(UTF16ToUTF8(
      input.text().substr(input.parts().host.begin, input.parts().host.len)));
  return (net::RegistryControlledDomainService::GetRegistryLength(host,
      false) == 0) && db->IsTypedHost(host);
}

bool HistoryURLProvider::PromoteMatchForInlineAutocomplete(
    HistoryURLProviderParams* params,
    const history::HistoryMatch& match,
    const history::HistoryMatches& matches) {
  // Promote the first match if it's been typed at least n times, where n == 1
  // for "simple" (host-only) URLs and n == 2 for others.  We set a higher bar
  // for these long URLs because it's less likely that users will want to visit
  // them again.  Even though we don't increment the typed_count for pasted-in
  // URLs, if the user manually edits the URL or types some long thing in by
  // hand, we wouldn't want to immediately start autocompleting it.
  if (!match.url_info.typed_count() ||
      ((match.url_info.typed_count() == 1) &&
       !IsHostOnly(match.url_info.url())))
    return false;

  // In the case where the user has typed "foo.com" and visited (but not typed)
  // "foo/", and the input is "foo", we can reach here for "foo.com" during the
  // first pass but have the second pass suggest the exact input as a better
  // URL.  Since we need both passes to agree, and since during the first pass
  // there's no way to know about "foo/", make reaching this point prevent any
  // future pass from suggesting the exact input as a better match.
  params->dont_suggest_exact_input = true;
  params->matches.push_back(HistoryMatchToACMatch(params, match, matches,
                                                  INLINE_AUTOCOMPLETE, 0));
  return true;
}

void HistoryURLProvider::SortMatches(history::HistoryMatches* matches) const {
  // Sort by quality, best first.
  std::sort(matches->begin(), matches->end(), &CompareHistoryMatch);

  // Remove duplicate matches (caused by the search string appearing in one of
  // the prefixes as well as after it).  Consider the following scenario:
  //
  // User has visited "http://http.com" once and "http://htaccess.com" twice.
  // User types "http".  The autocomplete search with prefix "http://" returns
  // the first host, while the search with prefix "" returns both hosts.  Now
  // we sort them into rank order:
  //   http://http.com     (innermost_match)
  //   http://htaccess.com (!innermost_match, url_info.visit_count == 2)
  //   http://http.com     (!innermost_match, url_info.visit_count == 1)
  //
  // The above scenario tells us we can't use std::unique(), since our
  // duplicates are not always sequential.  It also tells us we should remove
  // the lower-quality duplicate(s), since otherwise the returned results won't
  // be ordered correctly.  This is easy to do: we just always remove the later
  // element of a duplicate pair.
  // Be careful!  Because the vector contents may change as we remove elements,
  // we use an index instead of an iterator in the outer loop, and don't
  // precalculate the ending position.
  for (size_t i = 0; i < matches->size(); ++i) {
    for (history::HistoryMatches::iterator j(matches->begin() + i + 1);
         j != matches->end(); ) {
      if ((*matches)[i].url_info.url() == j->url_info.url())
        j = matches->erase(j);
      else
        ++j;
    }
  }
}

void HistoryURLProvider::CullPoorMatches(
    history::HistoryMatches* matches) const {
  const base::Time& threshold(history::AutocompleteAgeThreshold());
  for (history::HistoryMatches::iterator i(matches->begin());
       i != matches->end(); ) {
    if (RowQualifiesAsSignificant(i->url_info, threshold))
      ++i;
    else
      i = matches->erase(i);
  }
}

void HistoryURLProvider::CullRedirects(history::HistoryBackend* backend,
                                       history::HistoryMatches* matches,
                                       size_t max_results) const {
  for (size_t source = 0;
       (source < matches->size()) && (source < max_results); ) {
    const GURL& url = (*matches)[source].url_info.url();
    // TODO(brettw) this should go away when everything uses GURL.
    history::RedirectList redirects;
    backend->GetMostRecentRedirectsFrom(url, &redirects);
    if (!redirects.empty()) {
      // Remove all but the first occurrence of any of these redirects in the
      // search results. We also must add the URL we queried for, since it may
      // not be the first match and we'd want to remove it.
      //
      // For example, when A redirects to B and our matches are [A, X, B],
      // we'll get B as the redirects from, and we want to remove the second
      // item of that pair, removing B. If A redirects to B and our matches are
      // [B, X, A], we'll want to remove A instead.
      redirects.push_back(url);
      source = RemoveSubsequentMatchesOf(matches, source, redirects);
    } else {
      // Advance to next item.
      source++;
    }
  }

  if (matches->size() > max_results)
    matches->resize(max_results);
}

size_t HistoryURLProvider::RemoveSubsequentMatchesOf(
    history::HistoryMatches* matches,
    size_t source_index,
    const std::vector<GURL>& remove) const {
  size_t next_index = source_index + 1;  // return value = item after source

  // Find the first occurrence of any URL in the redirect chain. We want to
  // keep this one since it is rated the highest.
  history::HistoryMatches::iterator first(std::find_first_of(
      matches->begin(), matches->end(), remove.begin(), remove.end()));
  DCHECK(first != matches->end()) << "We should have always found at least the "
      "original URL.";

  // Find any following occurrences of any URL in the redirect chain, these
  // should be deleted.
  for (history::HistoryMatches::iterator next(std::find_first_of(first + 1,
           matches->end(), remove.begin(), remove.end()));
       next != matches->end(); next = std::find_first_of(next, matches->end(),
           remove.begin(), remove.end())) {
    // Remove this item. When we remove an item before the source index, we
    // need to shift it to the right and remember that so we can return it.
    next = matches->erase(next);
    if (static_cast<size_t>(next - matches->begin()) < next_index)
      --next_index;
  }
  return next_index;
}

AutocompleteMatch HistoryURLProvider::HistoryMatchToACMatch(
    HistoryURLProviderParams* params,
    const history::HistoryMatch& history_match,
    const history::HistoryMatches& history_matches,
    MatchType match_type,
    size_t match_number) {
  const history::URLRow& info = history_match.url_info;
  AutocompleteMatch match(this,
      CalculateRelevance(params->input.type(), match_type, match_number),
      !!info.visit_count(), AutocompleteMatch::HISTORY_URL);
  match.destination_url = info.url();
  DCHECK(match.destination_url.is_valid());
  size_t inline_autocomplete_offset =
      history_match.input_location + params->input.text().length();
  std::string languages = (match_type == WHAT_YOU_TYPED) ?
      std::string() : params->languages;
  const net::FormatUrlTypes format_types = net::kFormatUrlOmitAll &
      ~((params->trim_http && !history_match.match_in_scheme) ?
          0 : net::kFormatUrlOmitHTTP);
  match.fill_into_edit =
      AutocompleteInput::FormattedStringWithEquivalentMeaning(info.url(),
          net::FormatUrl(info.url(), languages, format_types,
                         net::UnescapeRule::SPACES, NULL, NULL,
                         &inline_autocomplete_offset));
  if (!params->prevent_inline_autocomplete)
    match.inline_autocomplete_offset = inline_autocomplete_offset;
  DCHECK((match.inline_autocomplete_offset == string16::npos) ||
         (match.inline_autocomplete_offset <= match.fill_into_edit.length()));

  size_t match_start = history_match.input_location;
  match.contents = net::FormatUrl(info.url(), languages,
      format_types, net::UnescapeRule::SPACES, NULL, NULL, &match_start);
  if ((match_start != string16::npos) &&
      (inline_autocomplete_offset != string16::npos) &&
      (inline_autocomplete_offset != match_start)) {
    DCHECK(inline_autocomplete_offset > match_start);
    AutocompleteMatch::ClassifyLocationInString(match_start,
        inline_autocomplete_offset - match_start, match.contents.length(),
        ACMatchClassification::URL, &match.contents_class);
  } else {
    AutocompleteMatch::ClassifyLocationInString(string16::npos, 0,
        match.contents.length(), ACMatchClassification::URL,
        &match.contents_class);
  }
  match.description = info.title();
  AutocompleteMatch::ClassifyMatchInString(params->input.text(),
                                           info.title(),
                                           ACMatchClassification::NONE,
                                           &match.description_class);

  return match;
}
