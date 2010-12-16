// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autocomplete/history_url_provider.h"

#include <algorithm>

#include "base/basictypes.h"
#include "base/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autocomplete/autocomplete_match.h"
#include "chrome/browser/history/history.h"
#include "chrome/browser/history/history_backend.h"
#include "chrome/browser/history/history_database.h"
#include "chrome/browser/net/url_fixer_upper.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "googleurl/src/gurl.h"
#include "googleurl/src/url_parse.h"
#include "googleurl/src/url_util.h"
#include "net/base/net_util.h"

using base::Time;
using base::TimeDelta;
using base::TimeTicks;
using history::Prefix;
using history::Prefixes;
using history::HistoryMatch;
using history::HistoryMatches;

namespace history {

// Returns true if |url| is just a host (e.g. "http://www.google.com/") and
// not some other subpage (e.g. "http://www.google.com/foo.html").
bool IsHostOnly(const GURL& url) {
  DCHECK(url.is_valid());
  return (!url.has_path() || (url.path() == "/")) && !url.has_query() &&
      !url.has_ref();
}

// Acts like the > operator for URLInfo classes.
bool CompareHistoryMatch(const HistoryMatch& a, const HistoryMatch& b) {
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
    const bool a_is_host_only = history::IsHostOnly(a.url_info.url());
    if (a_is_host_only != history::IsHostOnly(b.url_info.url()))
      return a_is_host_only;
  }

  // URLs that have been visited more often are better.
  if (a.url_info.visit_count() != b.url_info.visit_count())
    return a.url_info.visit_count() > b.url_info.visit_count();

  // URLs that have been visited more recently are better.
  return a.url_info.last_visit() > b.url_info.last_visit();
}

// Given the user's |input| and a |match| created from it, reduce the
// match's URL to just a host.  If this host still matches the user input,
// return it.  Returns the empty string on failure.
GURL ConvertToHostOnly(const HistoryMatch& match, const std::wstring& input) {
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

  const std::wstring spec = UTF8ToWide(host.spec());
  if (spec.compare(match.input_location, input.length(), input))
    return GURL();  // User typing is no longer a prefix.

  return host;
}

}  // namespace history

HistoryURLProviderParams::HistoryURLProviderParams(
    const AutocompleteInput& input,
    bool trim_http,
    const std::string& languages)
    : message_loop(MessageLoop::current()),
      input(input),
      trim_http(trim_http),
      cancel(false),
      failed(false),
      languages(languages) {
}

HistoryURLProviderParams::~HistoryURLProviderParams() {}

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
    params_->cancel = true;
}

// Called on the history thread.
void HistoryURLProvider::ExecuteWithDB(history::HistoryBackend* backend,
                                       history::URLDatabase* db,
                                       HistoryURLProviderParams* params) {
  // We may get called with a NULL database if it couldn't be properly
  // initialized.
  if (!db) {
    params->failed = true;
  } else if (!params->cancel) {
    TimeTicks beginning_time = TimeTicks::Now();

    DoAutocomplete(backend, db, params);

    UMA_HISTOGRAM_TIMES("Autocomplete.HistoryAsyncQueryTime",
                        TimeTicks::Now() - beginning_time);
  }

  // Return the results (if any) to the main thread.
  params->message_loop->PostTask(FROM_HERE, NewRunnableMethod(
      this, &HistoryURLProvider::QueryComplete, params));
}

// Used by both autocomplete passes, and therefore called on multiple different
// threads (though not simultaneously).
void HistoryURLProvider::DoAutocomplete(history::HistoryBackend* backend,
                                        history::URLDatabase* db,
                                        HistoryURLProviderParams* params) {
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
          !params->trim_http ||
          url_util::FindAndCompareScheme(WideToUTF8(params->input.text()),
                                         chrome::kHttpsScheme, NULL));
  AutocompleteMatch what_you_typed_match(SuggestExactInput(params->input,
                                                           params->trim_http));

  // Get the matching URLs from the DB
  typedef std::vector<history::URLRow> URLRowVector;
  URLRowVector url_matches;
  HistoryMatches history_matches;

  for (Prefixes::const_iterator i(prefixes_.begin()); i != prefixes_.end();
       ++i) {
    if (params->cancel)
      return;  // Canceled in the middle of a query, give up.
    // We only need kMaxMatches results in the end, but before we get there we
    // need to promote lower-quality matches that are prefixes of
    // higher-quality matches, and remove lower-quality redirects.  So we ask
    // for more results than we need, of every prefix type, in hopes this will
    // give us far more than enough to work with.  CullRedirects() will then
    // reduce the list to the best kMaxMatches results.
    db->AutocompleteForPrefix(WideToUTF16(i->prefix + params->input.text()),
                              kMaxMatches * 2, (backend == NULL), &url_matches);
    for (URLRowVector::const_iterator j(url_matches.begin());
         j != url_matches.end(); ++j) {
      const Prefix* best_prefix = BestPrefix(j->url(), std::wstring());
      DCHECK(best_prefix != NULL);
      history_matches.push_back(HistoryMatch(*j, i->prefix.length(),
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
      FixupExactSuggestion(db, params->input, &what_you_typed_match,
                           &history_matches)) {
    // Got an exact match for the user's input.  Treat it as the best match
    // regardless of the input type.
    exact_suggestion = 1;
    params->matches.push_back(what_you_typed_match);
  } else if (params->input.prevent_inline_autocomplete() ||
      history_matches.empty() ||
      !PromoteMatchForInlineAutocomplete(params, history_matches.front())) {
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
    const HistoryMatch& match = history_matches[i];
    DCHECK(!have_what_you_typed_match ||
           (match.url_info.url() !=
            GURL(params->matches.front().destination_url)));
    params->matches.push_back(HistoryMatchToACMatch(params, match, NORMAL,
        history_matches.size() - 1 - i));
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
  if (params->cancel)
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
    std::wstring display_string(StringForURLDisplay(url, false, false));
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
    const Prefix* best_prefix = BestPrefix(match.destination_url, input.text());
    // Because of the vagaries of GURL, it's possible for match.destination_url
    // to not contain the user's input at all.  In this case don't mark anything
    // as a match.
    const size_t match_location = (best_prefix == NULL) ?
        std::wstring::npos : best_prefix->prefix.length() - offset;
    AutocompleteMatch::ClassifyLocationInString(match_location,
                                                input.text().length(),
                                                match.contents.length(),
                                                ACMatchClassification::URL,
                                                &match.contents_class);

    match.is_history_what_you_typed_match = true;
  }

  return match;
}

bool HistoryURLProvider::FixupExactSuggestion(history::URLDatabase* db,
                                              const AutocompleteInput& input,
                                              AutocompleteMatch* match,
                                              HistoryMatches* matches) const {
  DCHECK(match != NULL);
  DCHECK(matches != NULL);

  // Tricky corner case: The user has visited intranet site "foo", but not
  // internet site "www.foo.com".  He types in foo (getting an exact match),
  // then tries to hit ctrl-enter.  When pressing ctrl, the what-you-typed
  // match ("www.foo.com") doesn't show up in history, and thus doesn't get a
  // promoted relevance, but a different match from the input ("foo") does, and
  // gets promoted for inline autocomplete.  Thus instead of getting
  // "www.foo.com", the user still gets "foo" (and, before hitting enter,
  // probably gets an odd-looking inline autocomplete of "/").
  //
  // We detect this crazy case as follows:
  // * If the what-you-typed match is not in the history DB,
  // * and the user has specified a TLD,
  // * and the input _without_ the TLD _is_ in the history DB,
  // * ...then just before pressing "ctrl" the best match we supplied was the
  //   what-you-typed match, so stick with it by promoting this.
  history::URLRow info;
  MatchType type = INLINE_AUTOCOMPLETE;
  if (!db->GetRowForURL(match->destination_url, &info)) {
    if (input.desired_tld().empty())
      return false;
    GURL destination_url(URLFixerUpper::FixupURL(WideToUTF8(input.text()),
                                                 std::string()));
    if (!db->GetRowForURL(destination_url, NULL))
      return false;

    // If we got here, then we hit the tricky corner case.  Make sure that
    // |info| corresponds to the right URL.
    info = history::URLRow(match->destination_url);
  } else {
    // We have data for this match, use it.
    match->deletable = true;
    match->description = UTF16ToWide(info.title());
    AutocompleteMatch::ClassifyMatchInString(input.text(),
        UTF16ToWide(info.title()),
        ACMatchClassification::NONE, &match->description_class);
    if (!info.typed_count()) {
      // If we reach here, we must be in the second pass, and we must not have
      // promoted this match as an exact match during the first pass.  That
      // means it will have been outscored by the "search what you typed match".
      // We need to maintain that ordering in order to not make the destination
      // for the user's typing change depending on when they hit enter.  So
      // lower the score here enough to let the search provider continue to
      // outscore this match.
      type = WHAT_YOU_TYPED;
    }
  }

  // Promote as an exact match.
  match->relevance = CalculateRelevance(input.type(), type, 0);

  // Put it on the front of the HistoryMatches for redirect culling.
  EnsureMatchPresent(info, std::wstring::npos, false, matches, true);
  return true;
}

bool HistoryURLProvider::PromoteMatchForInlineAutocomplete(
    HistoryURLProviderParams* params,
    const HistoryMatch& match) {
  // Promote the first match if it's been typed at least n times, where n == 1
  // for "simple" (host-only) URLs and n == 2 for others.  We set a higher bar
  // for these long URLs because it's less likely that users will want to visit
  // them again.  Even though we don't increment the typed_count for pasted-in
  // URLs, if the user manually edits the URL or types some long thing in by
  // hand, we wouldn't want to immediately start autocompleting it.
  if (!match.url_info.typed_count() ||
      ((match.url_info.typed_count() == 1) &&
       !history::IsHostOnly(match.url_info.url())))
    return false;

  params->matches.push_back(HistoryMatchToACMatch(params, match,
                                                  INLINE_AUTOCOMPLETE, 0));
  return true;
}

// static
history::Prefixes HistoryURLProvider::GetPrefixes() {
  // We'll complete text following these prefixes.
  // NOTE: There's no requirement that these be in any particular order.
  Prefixes prefixes;
  prefixes.push_back(Prefix(L"https://www.", 2));
  prefixes.push_back(Prefix(L"http://www.", 2));
  prefixes.push_back(Prefix(L"ftp://ftp.", 2));
  prefixes.push_back(Prefix(L"ftp://www.", 2));
  prefixes.push_back(Prefix(L"https://", 1));
  prefixes.push_back(Prefix(L"http://", 1));
  prefixes.push_back(Prefix(L"ftp://", 1));
  prefixes.push_back(Prefix(L"", 0));  // Catches within-scheme matches as well
  return prefixes;
}

// static
int HistoryURLProvider::CalculateRelevance(AutocompleteInput::Type input_type,
                                           MatchType match_type,
                                           size_t match_number) {
  switch (match_type) {
    case INLINE_AUTOCOMPLETE:
      return 1400;

    case WHAT_YOU_TYPED:
      return 1200;

    default:
      return 900 + static_cast<int>(match_number);
  }
}

// static
void HistoryURLProvider::PromoteOrCreateShorterSuggestion(
    history::URLDatabase* db,
    const HistoryURLProviderParams& params,
    bool have_what_you_typed_match,
    const AutocompleteMatch& what_you_typed_match,
    HistoryMatches* matches) {
  if (matches->empty())
    return;  // No matches, nothing to do.

  // Determine the base URL from which to search, and whether that URL could
  // itself be added as a match.  We can add the base iff it's not "effectively
  // the same" as any "what you typed" match.
  const HistoryMatch& match = matches->front();
  GURL search_base = history::ConvertToHostOnly(match, params.input.text());
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

// static
void HistoryURLProvider::EnsureMatchPresent(
    const history::URLRow& info,
    std::wstring::size_type input_location,
    bool match_in_scheme,
    HistoryMatches* matches,
    bool promote) {
  // |matches| may already have an entry for this.
  for (HistoryMatches::iterator i(matches->begin()); i != matches->end();
       ++i) {
    if (i->url_info.url() == info.url()) {
      // Rotate it to the front if the caller wishes.
      if (promote)
        std::rotate(matches->begin(), i, i + 1);
      return;
    }
  }

  // No entry, so create one.
  HistoryMatch match(info, input_location, match_in_scheme, true);
  if (promote)
    matches->push_front(match);
  else
    matches->push_back(match);
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
  HistoryService* const history_service =
      profile_->GetHistoryService(Profile::EXPLICIT_ACCESS);
  if (!history_service)
    return;

  // Create the data structure for the autocomplete passes.  We'll save this off
  // onto the |params_| member for later deletion below if we need to run pass
  // 2.
  std::string languages(languages_);
  if (languages.empty() && profile_) {
    languages =
        profile_->GetPrefs()->GetString(prefs::kAcceptLanguages);
  }
  scoped_ptr<HistoryURLProviderParams> params(
      new HistoryURLProviderParams(input, trim_http, languages));

  if (fixup_input_and_run_pass_1) {
    // Do some fixup on the user input before matching against it, so we provide
    // good results for local file paths, input with spaces, etc.
    // NOTE: This purposefully doesn't take input.desired_tld() into account; if
    // it did, then holding "ctrl" would change all the results from the
    // HistoryURLProvider provider, not just the What You Typed Result.
    const std::wstring fixed_text(FixupUserInput(input));
    if (fixed_text.empty()) {
      // Conceivably fixup could result in an empty string (although I don't
      // have cases where this happens offhand).  We can't do anything with
      // empty input, so just bail; otherwise we'd crash later.
      return;
    }
    params->input.set_text(fixed_text);

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
  if (!input.synchronous_only()) {
    done_ = false;
    params_ = params.release();  // This object will be destroyed in
                                 // QueryComplete() once we're done with it.
    history_service->ScheduleAutocomplete(this, params_);
  }
}

const history::Prefix* HistoryURLProvider::BestPrefix(
    const GURL& url,
    const std::wstring& prefix_suffix) const {
  const Prefix* best_prefix = NULL;
  const std::wstring text(UTF8ToWide(url.spec()));
  for (Prefixes::const_iterator i(prefixes_.begin()); i != prefixes_.end();
       ++i) {
    if ((best_prefix == NULL) ||
        (i->num_components > best_prefix->num_components)) {
      std::wstring prefix_with_suffix(i->prefix + prefix_suffix);
      if ((text.length() >= prefix_with_suffix.length()) &&
          !text.compare(0, prefix_with_suffix.length(), prefix_with_suffix))
        best_prefix = &(*i);
    }
  }
  return best_prefix;
}

void HistoryURLProvider::SortMatches(HistoryMatches* matches) const {
  // Sort by quality, best first.
  std::sort(matches->begin(), matches->end(), &history::CompareHistoryMatch);

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
    HistoryMatches::iterator j(matches->begin() + i + 1);
    while (j != matches->end()) {
      if ((*matches)[i].url_info.url() == j->url_info.url())
        j = matches->erase(j);
      else
        ++j;
    }
  }
}

void HistoryURLProvider::CullPoorMatches(HistoryMatches* matches) const {
  Time recent_threshold = history::AutocompleteAgeThreshold();
  for (HistoryMatches::iterator i(matches->begin()); i != matches->end();) {
    const history::URLRow& url_info(i->url_info);
    if ((url_info.typed_count() <= history::kLowQualityMatchTypedLimit) &&
        (url_info.visit_count() <= history::kLowQualityMatchVisitLimit) &&
        (url_info.last_visit() < recent_threshold)) {
      i = matches->erase(i);
    } else {
      ++i;
    }
  }
}

void HistoryURLProvider::CullRedirects(history::HistoryBackend* backend,
                                       HistoryMatches* matches,
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
    HistoryMatches* matches,
    size_t source_index,
    const std::vector<GURL>& remove) const {
  size_t next_index = source_index + 1;  // return value = item after source

  // Find the first occurrence of any URL in the redirect chain. We want to
  // keep this one since it is rated the highest.
  HistoryMatches::iterator first(std::find_first_of(
      matches->begin(), matches->end(), remove.begin(), remove.end()));
  DCHECK(first != matches->end()) <<
      "We should have always found at least the original URL.";

  // Find any following occurrences of any URL in the redirect chain, these
  // should be deleted.
  HistoryMatches::iterator next(first);
  next++;  // Start searching immediately after the one we found already.
  while (next != matches->end() &&
         (next = std::find_first_of(next, matches->end(), remove.begin(),
                                    remove.end())) != matches->end()) {
    // Remove this item. When we remove an item before the source index, we
    // need to shift it to the right and remember that so we can return it.
    next = matches->erase(next);
    if (static_cast<size_t>(next - matches->begin()) < next_index)
      next_index--;
  }
  return next_index;
}

AutocompleteMatch HistoryURLProvider::HistoryMatchToACMatch(
    HistoryURLProviderParams* params,
    const HistoryMatch& history_match,
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
          UTF16ToWideHack(net::FormatUrl(info.url(), languages, format_types,
                                         UnescapeRule::SPACES, NULL, NULL,
                                         &inline_autocomplete_offset)));
  if (!params->input.prevent_inline_autocomplete())
    match.inline_autocomplete_offset = inline_autocomplete_offset;
  DCHECK((match.inline_autocomplete_offset == std::wstring::npos) ||
         (match.inline_autocomplete_offset <= match.fill_into_edit.length()));

  size_t match_start = history_match.input_location;
  match.contents = UTF16ToWideHack(net::FormatUrl(info.url(), languages,
      format_types, UnescapeRule::SPACES, NULL, NULL, &match_start));
  if ((match_start != std::wstring::npos) &&
      (inline_autocomplete_offset != std::wstring::npos) &&
      (inline_autocomplete_offset != match_start)) {
    DCHECK(inline_autocomplete_offset > match_start);
    AutocompleteMatch::ClassifyLocationInString(match_start,
        inline_autocomplete_offset - match_start, match.contents.length(),
        ACMatchClassification::URL, &match.contents_class);
  } else {
    AutocompleteMatch::ClassifyLocationInString(std::wstring::npos, 0,
        match.contents.length(), ACMatchClassification::URL,
        &match.contents_class);
  }
  match.description = UTF16ToWide(info.title());
  AutocompleteMatch::ClassifyMatchInString(params->input.text(),
                                           UTF16ToWide(info.title()),
                                           ACMatchClassification::NONE,
                                           &match.description_class);

  return match;
}
