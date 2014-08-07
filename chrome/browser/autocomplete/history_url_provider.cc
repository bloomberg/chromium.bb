// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autocomplete/history_url_provider.h"

#include <algorithm>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/autocomplete/autocomplete_result.h"
#include "chrome/browser/autocomplete/chrome_autocomplete_scheme_classifier.h"
#include "chrome/browser/history/history_backend.h"
#include "chrome/browser/history/history_database.h"
#include "chrome/browser/history/history_service.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/history/history_types.h"
#include "chrome/browser/history/in_memory_url_index_types.h"
#include "chrome/browser/history/scored_history_match.h"
#include "chrome/browser/omnibox/omnibox_field_trial.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/search_engines/ui_thread_search_terms_data.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "components/autocomplete/autocomplete_match.h"
#include "components/autocomplete/autocomplete_provider_listener.h"
#include "components/autocomplete/url_prefix.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/metrics/proto/omnibox_input_type.pb.h"
#include "components/search_engines/template_url_service.h"
#include "components/url_fixer/url_fixer.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/net_util.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "url/gurl.h"
#include "url/url_parse.h"
#include "url/url_util.h"

namespace {

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
  if ((a.url_info.typed_count() == 1) && (a.IsHostOnly() != b.IsHostOnly()))
    return a.IsHostOnly();

  // URLs that have been visited more often are better.
  if (a.url_info.visit_count() != b.url_info.visit_count())
    return a.url_info.visit_count() > b.url_info.visit_count();

  // URLs that have been visited more recently are better.
  return a.url_info.last_visit() > b.url_info.last_visit();
}

// Sorts and dedups the given list of matches.
void SortAndDedupMatches(history::HistoryMatches* matches) {
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

// Calculates a new relevance score applying half-life time decaying to |count|
// using |time_since_last_visit| and |score_buckets|.  This function will never
// return a score higher than |undecayed_relevance|; in other words, it can only
// demote the old score.
double CalculateRelevanceUsingScoreBuckets(
    const HUPScoringParams::ScoreBuckets& score_buckets,
    const base::TimeDelta& time_since_last_visit,
    int undecayed_relevance,
    int count) {
  // Back off if above relevance cap.
  if ((score_buckets.relevance_cap() != -1) &&
      (undecayed_relevance >= score_buckets.relevance_cap()))
    return undecayed_relevance;

  // Time based decay using half-life time.
  double decayed_count = count;
  if (decayed_count > 0)
    decayed_count *= score_buckets.HalfLifeTimeDecay(time_since_last_visit);

  // Find a threshold where decayed_count >= bucket.
  const HUPScoringParams::ScoreBuckets::CountMaxRelevance* score_bucket = NULL;
  for (size_t i = 0; i < score_buckets.buckets().size(); ++i) {
    score_bucket = &score_buckets.buckets()[i];
    if (decayed_count >= score_bucket->first)
      break;  // Buckets are in descending order, so we can ignore the rest.
  }

  return (score_bucket && (undecayed_relevance > score_bucket->second)) ?
      score_bucket->second : undecayed_relevance;
}

// Returns a new relevance score for the given |match| based on the
// |old_relevance| score and |scoring_params|.  The new relevance score is
// guaranteed to be less than or equal to |old_relevance|.  In other words, this
// function can only demote a score, never boost it.  Returns |old_relevance| if
// experimental scoring is disabled.
int CalculateRelevanceScoreUsingScoringParams(
    const history::HistoryMatch& match,
    int old_relevance,
    const HUPScoringParams& scoring_params) {
  if (!scoring_params.experimental_scoring_enabled)
    return old_relevance;

  const base::TimeDelta time_since_last_visit =
      base::Time::Now() - match.url_info.last_visit();

  int relevance = CalculateRelevanceUsingScoreBuckets(
      scoring_params.typed_count_buckets, time_since_last_visit, old_relevance,
      match.url_info.typed_count());

  // Additional demotion (on top of typed_count demotion) of URLs that were
  // never typed.
  if (match.url_info.typed_count() == 0) {
    relevance = CalculateRelevanceUsingScoreBuckets(
        scoring_params.visited_count_buckets, time_since_last_visit, relevance,
        match.url_info.visit_count());
  }

  DCHECK_LE(relevance, old_relevance);
  return relevance;
}

// Extracts typed_count, visit_count, and last_visited time from the URLRow and
// puts them in the additional info field of the |match| for display in
// about:omnibox.
void RecordAdditionalInfoFromUrlRow(const history::URLRow& info,
                                    AutocompleteMatch* match) {
  match->RecordAdditionalInfo("typed count", info.typed_count());
  match->RecordAdditionalInfo("visit count", info.visit_count());
  match->RecordAdditionalInfo("last visit", info.last_visit());
}

// If |create_if_necessary| is true, ensures that |matches| contains an entry
// for |info|, creating a new such entry if necessary (using |input_location|
// and |match_in_scheme|).
//
// If |promote| is true, this also ensures the entry is the first element in
// |matches|, moving or adding it to the front as appropriate.  When |promote|
// is false, existing matches are left in place, and newly added matches are
// placed at the back.
//
// It's OK to call this function with both |create_if_necessary| and |promote|
// false, in which case we'll do nothing.
//
// Returns whether the match exists regardless if it was promoted/created.
bool CreateOrPromoteMatch(const history::URLRow& info,
                          size_t input_location,
                          bool match_in_scheme,
                          history::HistoryMatches* matches,
                          bool create_if_necessary,
                          bool promote) {
  // |matches| may already have an entry for this.
  for (history::HistoryMatches::iterator i(matches->begin());
       i != matches->end(); ++i) {
    if (i->url_info.url() == info.url()) {
      // Rotate it to the front if the caller wishes.
      if (promote)
        std::rotate(matches->begin(), i, i + 1);
      return true;
    }
  }

  if (!create_if_necessary)
    return false;

  // No entry, so create one.
  history::HistoryMatch match(info, input_location, match_in_scheme, true);
  if (promote)
    matches->push_front(match);
  else
    matches->push_back(match);

  return true;
}

// Returns whether |match| is suitable for inline autocompletion.
bool CanPromoteMatchForInlineAutocomplete(const history::HistoryMatch& match) {
  // We can promote this match if it's been typed at least n times, where n == 1
  // for "simple" (host-only) URLs and n == 2 for others.  We set a higher bar
  // for these long URLs because it's less likely that users will want to visit
  // them again.  Even though we don't increment the typed_count for pasted-in
  // URLs, if the user manually edits the URL or types some long thing in by
  // hand, we wouldn't want to immediately start autocompleting it.
  return match.url_info.typed_count() &&
      ((match.url_info.typed_count() > 1) || match.IsHostOnly());
}

// Given the user's |input| and a |match| created from it, reduce the match's
// URL to just a host.  If this host still matches the user input, return it.
// Returns the empty string on failure.
GURL ConvertToHostOnly(const history::HistoryMatch& match,
                       const base::string16& input) {
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

  const base::string16 spec = base::UTF8ToUTF16(host.spec());
  if (spec.compare(match.input_location, input.length(), input))
    return GURL();  // User typing is no longer a prefix.

  return host;
}

}  // namespace

// -----------------------------------------------------------------
// SearchTermsDataSnapshot

// Implementation of SearchTermsData that takes a snapshot of another
// SearchTermsData by copying all the responses to the different getters into
// member strings, then returning those strings when its own getters are called.
// This will typically be constructed on the UI thread from
// UIThreadSearchTermsData but is subsequently safe to use on any thread.
class SearchTermsDataSnapshot : public SearchTermsData {
 public:
  explicit SearchTermsDataSnapshot(const SearchTermsData& search_terms_data);
  virtual ~SearchTermsDataSnapshot();

  virtual std::string GoogleBaseURLValue() const OVERRIDE;
  virtual std::string GetApplicationLocale() const OVERRIDE;
  virtual base::string16 GetRlzParameterValue(
      bool from_app_list) const OVERRIDE;
  virtual std::string GetSearchClient() const OVERRIDE;
  virtual bool EnableAnswersInSuggest() const OVERRIDE;
  virtual bool IsShowingSearchTermsOnSearchResultsPages() const OVERRIDE;
  virtual std::string InstantExtendedEnabledParam(
      bool for_search) const OVERRIDE;
  virtual std::string ForceInstantResultsParam(
      bool for_prerender) const OVERRIDE;
  virtual std::string NTPIsThemedParam() const OVERRIDE;
  virtual std::string GoogleImageSearchSource() const OVERRIDE;

 private:
  std::string google_base_url_value_;
  std::string application_locale_;
  base::string16 rlz_parameter_value_;
  std::string search_client_;
  bool enable_answers_in_suggest_;
  bool is_showing_search_terms_on_search_results_pages_;
  std::string instant_extended_enabled_param_;
  std::string instant_extended_enabled_param_for_search_;
  std::string force_instant_results_param_;
  std::string force_instant_results_param_for_prerender_;
  std::string ntp_is_themed_param_;
  std::string google_image_search_source_;

  DISALLOW_COPY_AND_ASSIGN(SearchTermsDataSnapshot);
};

SearchTermsDataSnapshot::SearchTermsDataSnapshot(
    const SearchTermsData& search_terms_data)
    : google_base_url_value_(search_terms_data.GoogleBaseURLValue()),
      application_locale_(search_terms_data.GetApplicationLocale()),
      rlz_parameter_value_(search_terms_data.GetRlzParameterValue(false)),
      search_client_(search_terms_data.GetSearchClient()),
      enable_answers_in_suggest_(search_terms_data.EnableAnswersInSuggest()),
      is_showing_search_terms_on_search_results_pages_(
          search_terms_data.IsShowingSearchTermsOnSearchResultsPages()),
      instant_extended_enabled_param_(
          search_terms_data.InstantExtendedEnabledParam(false)),
      instant_extended_enabled_param_for_search_(
          search_terms_data.InstantExtendedEnabledParam(true)),
      force_instant_results_param_(
          search_terms_data.ForceInstantResultsParam(false)),
      force_instant_results_param_for_prerender_(
          search_terms_data.ForceInstantResultsParam(true)),
      ntp_is_themed_param_(search_terms_data.NTPIsThemedParam()),
      google_image_search_source_(search_terms_data.GoogleImageSearchSource()) {
}

SearchTermsDataSnapshot::~SearchTermsDataSnapshot() {
}

std::string SearchTermsDataSnapshot::GoogleBaseURLValue() const {
  return google_base_url_value_;
}

std::string SearchTermsDataSnapshot::GetApplicationLocale() const {
  return application_locale_;
}

base::string16 SearchTermsDataSnapshot::GetRlzParameterValue(
    bool from_app_list) const {
  return rlz_parameter_value_;
}

std::string SearchTermsDataSnapshot::GetSearchClient() const {
  return search_client_;
}

bool SearchTermsDataSnapshot::EnableAnswersInSuggest() const {
  return enable_answers_in_suggest_;
}

bool SearchTermsDataSnapshot::IsShowingSearchTermsOnSearchResultsPages() const {
  return is_showing_search_terms_on_search_results_pages_;
}

std::string SearchTermsDataSnapshot::InstantExtendedEnabledParam(
    bool for_search) const {
  return for_search ? instant_extended_enabled_param_ :
      instant_extended_enabled_param_for_search_;
}

std::string SearchTermsDataSnapshot::ForceInstantResultsParam(
    bool for_prerender) const {
  return for_prerender ? force_instant_results_param_ :
      force_instant_results_param_for_prerender_;
}

std::string SearchTermsDataSnapshot::NTPIsThemedParam() const {
  return ntp_is_themed_param_;
}

std::string SearchTermsDataSnapshot::GoogleImageSearchSource() const {
  return google_image_search_source_;
}

// -----------------------------------------------------------------
// HistoryURLProvider

// These ugly magic numbers will go away once we switch all scoring
// behavior (including URL-what-you-typed) to HistoryQuick provider.
const int HistoryURLProvider::kScoreForBestInlineableResult = 1413;
const int HistoryURLProvider::kScoreForUnvisitedIntranetResult = 1403;
const int HistoryURLProvider::kScoreForWhatYouTypedResult = 1203;
const int HistoryURLProvider::kBaseScoreForNonInlineableResult = 900;

// VisitClassifier is used to classify the type of visit to a particular url.
class HistoryURLProvider::VisitClassifier {
 public:
  enum Type {
    INVALID,             // Navigations to the URL are not allowed.
    UNVISITED_INTRANET,  // A navigable URL for which we have no visit data but
                         // which is known to refer to a visited intranet host.
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
      ((input.type() == metrics::OmniboxInputType::UNKNOWN) &&
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
  }
}

HistoryURLProviderParams::HistoryURLProviderParams(
    const AutocompleteInput& input,
    bool trim_http,
    const AutocompleteMatch& what_you_typed_match,
    const std::string& languages,
    TemplateURL* default_search_provider,
    const SearchTermsData& search_terms_data)
    : message_loop(base::MessageLoop::current()),
      input(input),
      prevent_inline_autocomplete(input.prevent_inline_autocomplete()),
      trim_http(trim_http),
      what_you_typed_match(what_you_typed_match),
      failed(false),
      exact_suggestion_is_in_history(false),
      promote_type(NEITHER),
      languages(languages),
      default_search_provider(default_search_provider ?
          new TemplateURL(default_search_provider->data()) : NULL),
      search_terms_data(new SearchTermsDataSnapshot(search_terms_data)) {
}

HistoryURLProviderParams::~HistoryURLProviderParams() {
}

HistoryURLProvider::HistoryURLProvider(AutocompleteProviderListener* listener,
                                       Profile* profile)
    : HistoryProvider(profile, AutocompleteProvider::TYPE_HISTORY_URL),
      listener_(listener),
      params_(NULL) {
  // Initialize HUP scoring params based on the current experiment.
  OmniboxFieldTrial::GetExperimentalHUPScoringParams(&scoring_params_);
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
  Stop(false);

  matches_.clear();

  if ((input.type() == metrics::OmniboxInputType::INVALID) ||
      (input.type() == metrics::OmniboxInputType::FORCED_QUERY))
    return;

  // Do some fixup on the user input before matching against it, so we provide
  // good results for local file paths, input with spaces, etc.
  const FixupReturn fixup_return(FixupUserInput(input));
  if (!fixup_return.first)
    return;
  url::Parsed parts;
  url_fixer::SegmentURL(fixup_return.second, &parts);
  AutocompleteInput fixed_up_input(input);
  fixed_up_input.UpdateText(fixup_return.second, base::string16::npos, parts);

  // Create a match for what the user typed.
  const bool trim_http = !AutocompleteInput::HasHTTPScheme(input.text());
  AutocompleteMatch what_you_typed_match(SuggestExactInput(
      fixed_up_input.text(), fixed_up_input.canonicalized_url(), trim_http));
  what_you_typed_match.relevance = CalculateRelevance(WHAT_YOU_TYPED, 0);

  // Add the WYT match as a fallback in case we can't get the history service or
  // URL DB; otherwise, we'll replace this match lower down.  Don't do this for
  // queries, though -- while we can sometimes mark up a match for them, it's
  // not what the user wants, and just adds noise.
  if (fixed_up_input.type() != metrics::OmniboxInputType::QUERY)
    matches_.push_back(what_you_typed_match);

  // We'll need the history service to run both passes, so try to obtain it.
  if (!profile_)
    return;
  HistoryService* const history_service =
      HistoryServiceFactory::GetForProfile(profile_, Profile::EXPLICIT_ACCESS);
  if (!history_service)
    return;

  // Get the default search provider and search terms data now since we have to
  // retrieve these on the UI thread, and the second pass runs on the history
  // thread. |template_url_service| can be NULL when testing.
  TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile_);
  TemplateURL* default_search_provider = template_url_service ?
      template_url_service->GetDefaultSearchProvider() : NULL;
  UIThreadSearchTermsData data(profile_);

  // Create the data structure for the autocomplete passes.  We'll save this off
  // onto the |params_| member for later deletion below if we need to run pass
  // 2.
  scoped_ptr<HistoryURLProviderParams> params(
      new HistoryURLProviderParams(
          fixed_up_input, trim_http, what_you_typed_match,
          profile_->GetPrefs()->GetString(prefs::kAcceptLanguages),
          default_search_provider, data));
  // Note that we use the non-fixed-up input here, since fixup may strip
  // trailing whitespace.
  params->prevent_inline_autocomplete = PreventInlineAutocomplete(input);

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
    matches_.clear();
    PromoteMatchesIfNecessary(*params);
    // NOTE: We don't reset |params| here since at least the |promote_type|
    // field on it will be read by the second pass -- see comments in
    // DoAutocomplete().
  }

  // Pass 2: Ask the history service to call us back on the history thread,
  // where we can read the full on-disk DB.
  if (input.want_asynchronous_matches()) {
    done_ = false;
    params_ = params.release();  // This object will be destroyed in
                                 // QueryComplete() once we're done with it.
    history_service->ScheduleAutocomplete(
        base::Bind(&HistoryURLProvider::ExecuteWithDB, this, params_));
  }
}

void HistoryURLProvider::Stop(bool clear_cached_results) {
  done_ = true;

  if (params_)
    params_->cancel_flag.Set();
}

AutocompleteMatch HistoryURLProvider::SuggestExactInput(
    const base::string16& text,
    const GURL& destination_url,
    bool trim_http) {
  // The FormattedStringWithEquivalentMeaning() call below requires callers to
  // be on the UI thread.
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI) ||
      !content::BrowserThread::IsThreadInitialized(content::BrowserThread::UI));

  AutocompleteMatch match(this, 0, false,
                          AutocompleteMatchType::URL_WHAT_YOU_TYPED);

  if (destination_url.is_valid()) {
    match.destination_url = destination_url;

    // Trim off "http://" if the user didn't type it.
    DCHECK(!trim_http || !AutocompleteInput::HasHTTPScheme(text));
    base::string16 display_string(
        net::FormatUrl(destination_url, std::string(),
                       net::kFormatUrlOmitAll & ~net::kFormatUrlOmitHTTP,
                       net::UnescapeRule::SPACES, NULL, NULL, NULL));
    const size_t offset = trim_http ? TrimHttpPrefix(&display_string) : 0;
    match.fill_into_edit =
        AutocompleteInput::FormattedStringWithEquivalentMeaning(
            destination_url,
            display_string,
            ChromeAutocompleteSchemeClassifier(profile_));
    match.allowed_to_be_default_match = true;
    // NOTE: Don't set match.inline_autocompletion to something non-empty here;
    // it's surprising and annoying.

    // Try to highlight "innermost" match location.  If we fix up "w" into
    // "www.w.com", we want to highlight the fifth character, not the first.
    // This relies on match.destination_url being the non-prefix-trimmed version
    // of match.contents.
    match.contents = display_string;
    const URLPrefix* best_prefix = URLPrefix::BestURLPrefix(
        base::UTF8ToUTF16(destination_url.spec()), text);
    // It's possible for match.destination_url to not contain the user's input
    // at all (so |best_prefix| is NULL), for example if the input is
    // "view-source:x" and |destination_url| has an inserted "http://" in the
    // middle.
    if (best_prefix == NULL) {
      AutocompleteMatch::ClassifyMatchInString(text, match.contents,
                                               ACMatchClassification::URL,
                                               &match.contents_class);
    } else {
      AutocompleteMatch::ClassifyLocationInString(
          best_prefix->prefix.length() - offset, text.length(),
          match.contents.length(), ACMatchClassification::URL,
          &match.contents_class);
    }

    match.is_history_what_you_typed_match = true;
  }

  return match;
}

void HistoryURLProvider::ExecuteWithDB(HistoryURLProviderParams* params,
                                       history::HistoryBackend* backend,
                                       history::URLDatabase* db) {
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
int HistoryURLProvider::CalculateRelevance(MatchType match_type,
                                           int match_number) {
  switch (match_type) {
    case INLINE_AUTOCOMPLETE:
      return kScoreForBestInlineableResult;

    case UNVISITED_INTRANET:
      return kScoreForUnvisitedIntranetResult;

    case WHAT_YOU_TYPED:
      return kScoreForWhatYouTypedResult;

    default:  // NORMAL
      return kBaseScoreForNonInlineableResult + match_number;
  }
}

// static
ACMatchClassifications HistoryURLProvider::ClassifyDescription(
    const base::string16& input_text,
    const base::string16& description) {
  base::string16 clean_description =
      bookmarks::CleanUpTitleForMatching(description);
  history::TermMatches description_matches(SortAndDeoverlapMatches(
      history::MatchTermInString(input_text, clean_description, 0)));
  history::WordStarts description_word_starts;
  history::String16VectorFromString16(
      clean_description, false, &description_word_starts);
  // If HistoryURL retrieves any matches (and hence we reach this code), we
  // are guaranteed that the beginning of input_text must be a word break.
  history::WordStarts offsets(1, 0u);
  description_matches =
      history::ScoredHistoryMatch::FilterTermMatchesByWordStarts(
          description_matches, offsets, description_word_starts, 0,
          std::string::npos);
  return SpansFromTermMatch(
      description_matches, clean_description.length(), false);
}

void HistoryURLProvider::DoAutocomplete(history::HistoryBackend* backend,
                                        history::URLDatabase* db,
                                        HistoryURLProviderParams* params) {
  // Get the matching URLs from the DB.
  params->matches.clear();
  history::URLRows url_matches;
  const URLPrefixes& prefixes = URLPrefix::GetURLPrefixes();
  for (URLPrefixes::const_iterator i(prefixes.begin()); i != prefixes.end();
       ++i) {
    if (params->cancel_flag.IsSet())
      return;  // Canceled in the middle of a query, give up.

    // We only need kMaxMatches results in the end, but before we get there we
    // need to promote lower-quality matches that are prefixes of higher-quality
    // matches, and remove lower-quality redirects.  So we ask for more results
    // than we need, of every prefix type, in hopes this will give us far more
    // than enough to work with.  CullRedirects() will then reduce the list to
    // the best kMaxMatches results.
    db->AutocompleteForPrefix(
        base::UTF16ToUTF8(i->prefix + params->input.text()), kMaxMatches * 2,
        !backend, &url_matches);
    for (history::URLRows::const_iterator j(url_matches.begin());
          j != url_matches.end(); ++j) {
      const URLPrefix* best_prefix = URLPrefix::BestURLPrefix(
          base::UTF8ToUTF16(j->url().spec()), base::string16());
      DCHECK(best_prefix);
      params->matches.push_back(history::HistoryMatch(
          *j, i->prefix.length(), !i->num_components,
          i->num_components >= best_prefix->num_components));
    }
  }

  // Create sorted list of suggestions.
  CullPoorMatches(params);
  SortAndDedupMatches(&params->matches);

  // Try to create a shorter suggestion from the best match.
  // We consider the what you typed match eligible for display when there's a
  // reasonable chance the user actually cares:
  // * Their input can be opened as a URL, and
  // * We parsed the input as a URL, or it starts with an explicit "http:" or
  // "https:".
  // Otherwise, this is just low-quality noise.  In the cases where we've parsed
  // as UNKNOWN, we'll still show an accidental search infobar if need be.
  VisitClassifier classifier(this, params->input, db);
  params->have_what_you_typed_match =
      (params->input.type() != metrics::OmniboxInputType::QUERY) &&
      ((params->input.type() != metrics::OmniboxInputType::UNKNOWN) ||
       (classifier.type() == VisitClassifier::UNVISITED_INTRANET) ||
       !params->trim_http ||
       (AutocompleteInput::NumNonHostComponents(params->input.parts()) > 0));
  const bool have_shorter_suggestion_suitable_for_inline_autocomplete =
      PromoteOrCreateShorterSuggestion(
          db, params->have_what_you_typed_match, params);

  // Check whether what the user typed appears in history.
  const bool can_check_history_for_exact_match =
      // Checking what_you_typed_match.is_history_what_you_typed_match tells us
      // whether SuggestExactInput() succeeded in constructing a valid match.
      params->what_you_typed_match.is_history_what_you_typed_match &&
      // Additionally, in the case where the user has typed "foo.com" and
      // visited (but not typed) "foo/", and the input is "foo", the first pass
      // will fall into the FRONT_HISTORY_MATCH case for "foo.com" but the
      // second pass can suggest the exact input as a better URL.  Since we need
      // both passes to agree, and since during the first pass there's no way to
      // know about "foo/", ensure that if the promote type was set to
      // FRONT_HISTORY_MATCH during the first pass, the second pass will not
      // consider the exact suggestion to be in history and therefore will not
      // suggest the exact input as a better match.  (Note that during the first
      // pass, this conditional will always succeed since |promote_type| is
      // initialized to NEITHER.)
      (params->promote_type != HistoryURLProviderParams::FRONT_HISTORY_MATCH);
  params->exact_suggestion_is_in_history = can_check_history_for_exact_match &&
      FixupExactSuggestion(db, classifier, params);

  // If we succeeded in fixing up the exact match based on the user's history,
  // we should treat it as the best match regardless of input type.  If not,
  // then we check whether there's an inline autocompletion we can create from
  // this input, so we can promote that as the best match.
  if (params->exact_suggestion_is_in_history) {
    params->promote_type = HistoryURLProviderParams::WHAT_YOU_TYPED_MATCH;
  } else if (!params->prevent_inline_autocomplete && !params->matches.empty() &&
      (have_shorter_suggestion_suitable_for_inline_autocomplete ||
       CanPromoteMatchForInlineAutocomplete(params->matches[0]))) {
    params->promote_type = HistoryURLProviderParams::FRONT_HISTORY_MATCH;
  } else {
    // Failed to promote any URLs.  Use the What You Typed match, if we have it.
    params->promote_type = params->have_what_you_typed_match ?
        HistoryURLProviderParams::WHAT_YOU_TYPED_MATCH :
        HistoryURLProviderParams::NEITHER;
  }

  const size_t max_results =
      kMaxMatches + (params->exact_suggestion_is_in_history ? 1 : 0);
  if (backend) {
    // Remove redirects and trim list to size.  We want to provide up to
    // kMaxMatches results plus the What You Typed result, if it was added to
    // params->matches above.
    CullRedirects(backend, &params->matches, max_results);
  } else if (params->matches.size() > max_results) {
    // Simply trim the list to size.
    params->matches.resize(max_results);
  }
}

void HistoryURLProvider::PromoteMatchesIfNecessary(
    const HistoryURLProviderParams& params) {
  if (params.promote_type == HistoryURLProviderParams::FRONT_HISTORY_MATCH) {
    matches_.push_back(HistoryMatchToACMatch(params, 0, INLINE_AUTOCOMPLETE,
        CalculateRelevance(INLINE_AUTOCOMPLETE, 0)));
    if (OmniboxFieldTrial::AddUWYTMatchEvenIfPromotedURLs() &&
        params.have_what_you_typed_match) {
      matches_.push_back(params.what_you_typed_match);
    }
  } else if (params.promote_type ==
      HistoryURLProviderParams::WHAT_YOU_TYPED_MATCH) {
    matches_.push_back(params.what_you_typed_match);
  }
}

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
    matches_.clear();
    PromoteMatchesIfNecessary(*params);

    // Determine relevance of highest scoring match, if any.
    int relevance = matches_.empty() ?
        CalculateRelevance(NORMAL,
                           static_cast<int>(params->matches.size() - 1)) :
        matches_[0].relevance;

    // Convert the history matches to autocomplete matches.  If we promoted the
    // first match, skip over it.
    const size_t first_match =
        (params->exact_suggestion_is_in_history ||
         (params->promote_type ==
             HistoryURLProviderParams::FRONT_HISTORY_MATCH)) ? 1 : 0;
    for (size_t i = first_match; i < params->matches.size(); ++i) {
      // All matches score one less than the previous match.
      --relevance;
      // The experimental scoring must not change the top result's score.
      if (!matches_.empty()) {
        relevance = CalculateRelevanceScoreUsingScoringParams(
            params->matches[i], relevance, scoring_params_);
      }
      matches_.push_back(HistoryMatchToACMatch(*params, i, NORMAL, relevance));
    }
  }

  done_ = true;
  listener_->OnProviderUpdate(true);
}

bool HistoryURLProvider::FixupExactSuggestion(
    history::URLDatabase* db,
    const VisitClassifier& classifier,
    HistoryURLProviderParams* params) const {
  MatchType type = INLINE_AUTOCOMPLETE;
  switch (classifier.type()) {
    case VisitClassifier::INVALID:
      return false;
    case VisitClassifier::UNVISITED_INTRANET:
      type = UNVISITED_INTRANET;
      break;
    default:
      DCHECK_EQ(VisitClassifier::VISITED, classifier.type());
      // We have data for this match, use it.
      params->what_you_typed_match.deletable = true;
      params->what_you_typed_match.description = classifier.url_row().title();
      RecordAdditionalInfoFromUrlRow(classifier.url_row(),
                                     &params->what_you_typed_match);
      params->what_you_typed_match.description_class = ClassifyDescription(
          params->input.text(), params->what_you_typed_match.description);
      if (!classifier.url_row().typed_count()) {
        // If we reach here, we must be in the second pass, and we must not have
        // this row's data available during the first pass.  That means we
        // either scored it as WHAT_YOU_TYPED or UNVISITED_INTRANET, and to
        // maintain the ordering between passes consistent, we need to score it
        // the same way here.
        type = CanFindIntranetURL(db, params->input) ?
            UNVISITED_INTRANET : WHAT_YOU_TYPED;
      }
      break;
  }

  const GURL& url = params->what_you_typed_match.destination_url;
  const url::Parsed& parsed = url.parsed_for_possibly_invalid_spec();
  // If the what-you-typed result looks like a single word (which can be
  // interpreted as an intranet address) followed by a pound sign ("#"),
  // leave the score for the url-what-you-typed result as is.  It will be
  // outscored by a search query from the SearchProvider. This test fixes
  // cases such as "c#" and "c# foo" where the user has visited an intranet
  // site "c".  We want the search-what-you-typed score to beat the
  // URL-what-you-typed score in this case.  Most of the below test tries to
  // make sure that this code does not trigger if the user did anything to
  // indicate the desired match is a URL.  For instance, "c/# foo" will not
  // pass the test because that will be classified as input type URL.  The
  // parsed.CountCharactersBefore() in the test looks for the presence of a
  // reference fragment in the URL by checking whether the position differs
  // included the delimiter (pound sign) versus not including the delimiter.
  // (One cannot simply check url.ref() because it will not distinguish
  // between the input "c" and the input "c#", both of which will have empty
  // reference fragments.)
  if ((type == UNVISITED_INTRANET) &&
      (params->input.type() != metrics::OmniboxInputType::URL) &&
      url.username().empty() && url.password().empty() && url.port().empty() &&
      (url.path() == "/") && url.query().empty() &&
      (parsed.CountCharactersBefore(url::Parsed::REF, true) !=
       parsed.CountCharactersBefore(url::Parsed::REF, false))) {
    return false;
  }

  params->what_you_typed_match.relevance = CalculateRelevance(type, 0);

  // If there are any other matches, then don't promote this match here, in
  // hopes the caller will be able to inline autocomplete a better suggestion.
  // DoAutocomplete() will fall back on this match if inline autocompletion
  // fails.  This matches how we react to never-visited URL inputs in the non-
  // intranet case.
  if (type == UNVISITED_INTRANET && !params->matches.empty())
    return false;

  // Put it on the front of the HistoryMatches for redirect culling.
  CreateOrPromoteMatch(classifier.url_row(), base::string16::npos, false,
                       &params->matches, true, true);
  return true;
}

bool HistoryURLProvider::CanFindIntranetURL(
    history::URLDatabase* db,
    const AutocompleteInput& input) const {
  // Normally passing the first two conditions below ought to guarantee the
  // third condition, but because FixupUserInput() can run and modify the
  // input's text and parts between Parse() and here, it seems better to be
  // paranoid and check.
  if ((input.type() != metrics::OmniboxInputType::UNKNOWN) ||
      !LowerCaseEqualsASCII(input.scheme(), url::kHttpScheme) ||
      !input.parts().host.is_nonempty())
    return false;
  const std::string host(base::UTF16ToUTF8(
      input.text().substr(input.parts().host.begin, input.parts().host.len)));
  const size_t registry_length =
      net::registry_controlled_domains::GetRegistryLength(
          host,
          net::registry_controlled_domains::EXCLUDE_UNKNOWN_REGISTRIES,
          net::registry_controlled_domains::EXCLUDE_PRIVATE_REGISTRIES);
  return registry_length == 0 && db->IsTypedHost(host);
}

bool HistoryURLProvider::PromoteOrCreateShorterSuggestion(
    history::URLDatabase* db,
    bool have_what_you_typed_match,
    HistoryURLProviderParams* params) {
  if (params->matches.empty())
    return false;  // No matches, nothing to do.

  // Determine the base URL from which to search, and whether that URL could
  // itself be added as a match.  We can add the base iff it's not "effectively
  // the same" as any "what you typed" match.
  const history::HistoryMatch& match = params->matches[0];
  GURL search_base = ConvertToHostOnly(match, params->input.text());
  bool can_add_search_base_to_matches = !have_what_you_typed_match;
  if (search_base.is_empty()) {
    // Search from what the user typed when we couldn't reduce the best match
    // to a host.  Careful: use a substring of |match| here, rather than the
    // first match in |params|, because they might have different prefixes.  If
    // the user typed "google.com", params->what_you_typed_match will hold
    // "http://google.com/", but |match| might begin with
    // "http://www.google.com/".
    // TODO: this should be cleaned up, and is probably incorrect for IDN.
    std::string new_match = match.url_info.url().possibly_invalid_spec().
        substr(0, match.input_location + params->input.text().length());
    search_base = GURL(new_match);
    if (search_base.is_empty())
      return false;  // Can't construct a URL from which to start a search.
  } else if (!can_add_search_base_to_matches) {
    can_add_search_base_to_matches =
        (search_base != params->what_you_typed_match.destination_url);
  }
  if (search_base == match.url_info.url())
    return false;  // Couldn't shorten |match|, so no URLs to search over.

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
      return false;  // Couldn't find anything and can't add the search base.

    // Try to get info on the search base itself.  Promote it to the top if the
    // original best match isn't good enough to autocomplete.
    db->GetRowForURL(search_base, &info);
    promote = match.url_info.typed_count() <= 1;
  }

  // Promote or add the desired URL to the list of matches.
  const bool ensure_can_inline =
      promote && CanPromoteMatchForInlineAutocomplete(match);
  return CreateOrPromoteMatch(info, match.input_location, match.match_in_scheme,
                              &params->matches, true, promote) &&
      ensure_can_inline;
}

void HistoryURLProvider::CullPoorMatches(
    HistoryURLProviderParams* params) const {
  const base::Time& threshold(history::AutocompleteAgeThreshold());
  for (history::HistoryMatches::iterator i(params->matches.begin());
       i != params->matches.end(); ) {
    if (RowQualifiesAsSignificant(i->url_info, threshold) &&
        (!params->default_search_provider ||
         !params->default_search_provider->IsSearchURL(
             i->url_info.url(), *params->search_terms_data))) {
      ++i;
    } else {
      i = params->matches.erase(i);
    }
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
    backend->QueryRedirectsFrom(url, &redirects);
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
      matches->begin(), matches->end(), remove.begin(), remove.end(),
      history::HistoryMatch::EqualsGURL));
  DCHECK(first != matches->end()) << "We should have always found at least the "
      "original URL.";

  // Find any following occurrences of any URL in the redirect chain, these
  // should be deleted.
  for (history::HistoryMatches::iterator next(std::find_first_of(first + 1,
           matches->end(), remove.begin(), remove.end(),
           history::HistoryMatch::EqualsGURL));
       next != matches->end(); next = std::find_first_of(next, matches->end(),
           remove.begin(), remove.end(), history::HistoryMatch::EqualsGURL)) {
    // Remove this item. When we remove an item before the source index, we
    // need to shift it to the right and remember that so we can return it.
    next = matches->erase(next);
    if (static_cast<size_t>(next - matches->begin()) < next_index)
      --next_index;
  }
  return next_index;
}

AutocompleteMatch HistoryURLProvider::HistoryMatchToACMatch(
    const HistoryURLProviderParams& params,
    size_t match_number,
    MatchType match_type,
    int relevance) {
  // The FormattedStringWithEquivalentMeaning() call below requires callers to
  // be on the UI thread.
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI) ||
      !content::BrowserThread::IsThreadInitialized(content::BrowserThread::UI));

  const history::HistoryMatch& history_match = params.matches[match_number];
  const history::URLRow& info = history_match.url_info;
  AutocompleteMatch match(this, relevance,
      !!info.visit_count(), AutocompleteMatchType::HISTORY_URL);
  match.typed_count = info.typed_count();
  match.destination_url = info.url();
  DCHECK(match.destination_url.is_valid());
  size_t inline_autocomplete_offset =
      history_match.input_location + params.input.text().length();
  std::string languages = (match_type == WHAT_YOU_TYPED) ?
      std::string() : params.languages;
  const net::FormatUrlTypes format_types = net::kFormatUrlOmitAll &
      ~((params.trim_http && !history_match.match_in_scheme) ?
          0 : net::kFormatUrlOmitHTTP);
  match.fill_into_edit =
      AutocompleteInput::FormattedStringWithEquivalentMeaning(
          info.url(),
          net::FormatUrl(info.url(), languages, format_types,
                         net::UnescapeRule::SPACES, NULL, NULL,
                         &inline_autocomplete_offset),
          ChromeAutocompleteSchemeClassifier(profile_));
  if (!params.prevent_inline_autocomplete &&
      (inline_autocomplete_offset != base::string16::npos)) {
    DCHECK(inline_autocomplete_offset <= match.fill_into_edit.length());
    match.inline_autocompletion =
        match.fill_into_edit.substr(inline_autocomplete_offset);
  }
  // The latter part of the test effectively asks "is the inline completion
  // empty?" (i.e., is this match effectively the what-you-typed match?).
  match.allowed_to_be_default_match = !params.prevent_inline_autocomplete ||
      ((inline_autocomplete_offset != base::string16::npos) &&
       (inline_autocomplete_offset >= match.fill_into_edit.length()));

  size_t match_start = history_match.input_location;
  match.contents = net::FormatUrl(info.url(), languages,
      format_types, net::UnescapeRule::SPACES, NULL, NULL, &match_start);
  if ((match_start != base::string16::npos) &&
      (inline_autocomplete_offset != base::string16::npos) &&
      (inline_autocomplete_offset != match_start)) {
    DCHECK(inline_autocomplete_offset > match_start);
    AutocompleteMatch::ClassifyLocationInString(match_start,
        inline_autocomplete_offset - match_start, match.contents.length(),
        ACMatchClassification::URL, &match.contents_class);
  } else {
    AutocompleteMatch::ClassifyLocationInString(base::string16::npos, 0,
        match.contents.length(), ACMatchClassification::URL,
        &match.contents_class);
  }
  match.description = info.title();
  match.description_class =
      ClassifyDescription(params.input.text(), match.description);
  RecordAdditionalInfoFromUrlRow(info, &match);
  return match;
}
