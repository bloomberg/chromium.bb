// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autocomplete/autocomplete_controller.h"

#include <set>
#include <string>

#include "base/format_macros.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "chrome/browser/autocomplete/autocomplete_controller_delegate.h"
#include "chrome/browser/autocomplete/bookmark_provider.h"
#include "chrome/browser/autocomplete/builtin_provider.h"
#include "chrome/browser/autocomplete/history_quick_provider.h"
#include "chrome/browser/autocomplete/history_url_provider.h"
#include "chrome/browser/autocomplete/keyword_provider.h"
#include "chrome/browser/autocomplete/search_provider.h"
#include "chrome/browser/autocomplete/shortcuts_provider.h"
#include "chrome/browser/autocomplete/zero_suggest_provider.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/omnibox/omnibox_field_trial.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/notification_service.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

// Converts the given match to a type (and possibly subtype) based on the AQS
// specification. For more details, see
// http://goto.google.com/binary-clients-logging.
void AutocompleteMatchToAssistedQuery(
    const AutocompleteMatch::Type& match,
    const AutocompleteProvider* provider,
    size_t* type,
    size_t* subtype) {
  // This type indicates a native chrome suggestion.
  *type = 69;
  // Default value, indicating no subtype.
  *subtype = base::string16::npos;

  // If provider is TYPE_ZERO_SUGGEST, set the subtype accordingly.
  // Type will be set in the switch statement below where we'll enter one of
  // SEARCH_SUGGEST or NAVSUGGEST. This subtype indicates context-aware zero
  // suggest.
  if (provider &&
      (provider->type() == AutocompleteProvider::TYPE_ZERO_SUGGEST) &&
      (match != AutocompleteMatchType::SEARCH_SUGGEST_PERSONALIZED)) {
    DCHECK((match == AutocompleteMatchType::SEARCH_SUGGEST) ||
           (match == AutocompleteMatchType::NAVSUGGEST));
    *subtype = 66;
  }

  switch (match) {
    case AutocompleteMatchType::SEARCH_SUGGEST: {
      // Do not set subtype here; subtype may have been set above.
      *type = 0;
      return;
    }
    case AutocompleteMatchType::SEARCH_SUGGEST_ENTITY: {
      *subtype = 46;
      return;
    }
    case AutocompleteMatchType::SEARCH_SUGGEST_INFINITE: {
      *subtype = 33;
      return;
    }
    case AutocompleteMatchType::SEARCH_SUGGEST_PERSONALIZED: {
      *subtype = 39;
      return;
    }
    case AutocompleteMatchType::SEARCH_SUGGEST_PROFILE: {
      *subtype = 44;
      return;
    }
    case AutocompleteMatchType::SEARCH_SUGGEST_ANSWER: {
      *subtype = 70;
      return;
    }
    case AutocompleteMatchType::NAVSUGGEST: {
      // Do not set subtype here; subtype may have been set above.
      *type = 5;
      return;
    }
    case AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED: {
      *subtype = 57;
      return;
    }
    case AutocompleteMatchType::URL_WHAT_YOU_TYPED: {
      *subtype = 58;
      return;
    }
    case AutocompleteMatchType::SEARCH_HISTORY: {
      *subtype = 59;
      return;
    }
    case AutocompleteMatchType::HISTORY_URL: {
      *subtype = 60;
      return;
    }
    case AutocompleteMatchType::HISTORY_TITLE: {
      *subtype = 61;
      return;
    }
    case AutocompleteMatchType::HISTORY_BODY: {
      *subtype = 62;
      return;
    }
    case AutocompleteMatchType::HISTORY_KEYWORD: {
      *subtype = 63;
      return;
    }
    case AutocompleteMatchType::BOOKMARK_TITLE: {
      *subtype = 65;
      return;
    }
    case AutocompleteMatchType::NAVSUGGEST_PERSONALIZED: {
      *subtype = 39;
      return;
    }
    default: {
      // This value indicates a native chrome suggestion with no named subtype
      // (yet).
      *subtype = 64;
    }
  }
}

// Appends available autocompletion of the given type, subtype, and number to
// the existing available autocompletions string, encoding according to the
// spec.
void AppendAvailableAutocompletion(size_t type,
                                   size_t subtype,
                                   int count,
                                   std::string* autocompletions) {
  if (!autocompletions->empty())
    autocompletions->append("j");
  base::StringAppendF(autocompletions, "%" PRIuS, type);
  // Subtype is optional - base::string16::npos indicates no subtype.
  if (subtype != base::string16::npos)
    base::StringAppendF(autocompletions, "i%" PRIuS, subtype);
  if (count > 1)
    base::StringAppendF(autocompletions, "l%d", count);
}

// Returns whether the autocompletion is trivial enough that we consider it
// an autocompletion for which the omnibox autocompletion code did not add
// any value.
bool IsTrivialAutocompletion(const AutocompleteMatch& match) {
  return match.type == AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED ||
      match.type == AutocompleteMatchType::URL_WHAT_YOU_TYPED ||
      match.type == AutocompleteMatchType::SEARCH_OTHER_ENGINE;
}

// Whether this autocomplete match type supports custom descriptions.
bool AutocompleteMatchHasCustomDescription(const AutocompleteMatch& match) {
  return match.type == AutocompleteMatchType::SEARCH_SUGGEST_ENTITY ||
      match.type == AutocompleteMatchType::SEARCH_SUGGEST_PROFILE;
}

}  // namespace

AutocompleteController::AutocompleteController(
    Profile* profile,
    TemplateURLService* template_url_service,
    AutocompleteControllerDelegate* delegate,
    int provider_types)
    : delegate_(delegate),
      history_url_provider_(NULL),
      keyword_provider_(NULL),
      search_provider_(NULL),
      zero_suggest_provider_(NULL),
      stop_timer_duration_(OmniboxFieldTrial::StopTimerFieldTrialDuration()),
      done_(true),
      in_start_(false),
      template_url_service_(template_url_service) {
  provider_types &= ~OmniboxFieldTrial::GetDisabledProviderTypes();
  if (provider_types & AutocompleteProvider::TYPE_BOOKMARK)
    providers_.push_back(new BookmarkProvider(profile));
  if (provider_types & AutocompleteProvider::TYPE_BUILTIN)
    providers_.push_back(new BuiltinProvider());
  if (provider_types & AutocompleteProvider::TYPE_HISTORY_QUICK)
    providers_.push_back(new HistoryQuickProvider(profile));
  if (provider_types & AutocompleteProvider::TYPE_HISTORY_URL) {
    history_url_provider_ = new HistoryURLProvider(this, profile);
    providers_.push_back(history_url_provider_);
  }
  // "Tab to search" can be used on all platforms other than Android.
#if !defined(OS_ANDROID)
  if (provider_types & AutocompleteProvider::TYPE_KEYWORD) {
    keyword_provider_ = new KeywordProvider(this, profile);
    providers_.push_back(keyword_provider_);
  }
#endif
  if (provider_types & AutocompleteProvider::TYPE_SEARCH) {
    search_provider_ = new SearchProvider(this, profile);
    providers_.push_back(search_provider_);
  }
  if (provider_types & AutocompleteProvider::TYPE_SHORTCUTS)
    providers_.push_back(new ShortcutsProvider(profile));
  if (provider_types & AutocompleteProvider::TYPE_ZERO_SUGGEST) {
    zero_suggest_provider_ = ZeroSuggestProvider::Create(this, profile);
    if (zero_suggest_provider_)
      providers_.push_back(zero_suggest_provider_);
  }
}

AutocompleteController::~AutocompleteController() {
  // The providers may have tasks outstanding that hold refs to them.  We need
  // to ensure they won't call us back if they outlive us.  (Practically,
  // calling Stop() should also cancel those tasks and make it so that we hold
  // the only refs.)  We also don't want to bother notifying anyone of our
  // result changes here, because the notification observer is in the midst of
  // shutdown too, so we don't ask Stop() to clear |result_| (and notify).
  result_.Reset();  // Not really necessary.
  Stop(false);
}

void AutocompleteController::Start(const AutocompleteInput& input) {
  const base::string16 old_input_text(input_.text());
  const bool old_want_asynchronous_matches = input_.want_asynchronous_matches();
  input_ = input;

  // See if we can avoid rerunning autocomplete when the query hasn't changed
  // much.  When the user presses or releases the ctrl key, the desired_tld
  // changes, and when the user finishes an IME composition, inline autocomplete
  // may no longer be prevented.  In both these cases the text itself hasn't
  // changed since the last query, and some providers can do much less work (and
  // get matches back more quickly).  Taking advantage of this reduces flicker.
  //
  // NOTE: This comes after constructing |input_| above since that construction
  // can change the text string (e.g. by stripping off a leading '?').
  const bool minimal_changes = (input_.text() == old_input_text) &&
      (input_.want_asynchronous_matches() == old_want_asynchronous_matches);

  expire_timer_.Stop();
  stop_timer_.Stop();

  // Start the new query.
  in_start_ = true;
  base::TimeTicks start_time = base::TimeTicks::Now();
  for (Providers::iterator i(providers_.begin()); i != providers_.end(); ++i) {
    // TODO(mpearson): Remove timing code once bugs 178705 / 237703 / 168933
    // are resolved.
    base::TimeTicks provider_start_time = base::TimeTicks::Now();

    // Call Start() on ZeroSuggestProvider with an INVALID AutocompleteInput
    // to clear out zero-suggest |matches_|.
    if (*i == zero_suggest_provider_)
      (*i)->Start(AutocompleteInput(), minimal_changes);
    else
      (*i)->Start(input_, minimal_changes);

    if (!input.want_asynchronous_matches())
      DCHECK((*i)->done());
    base::TimeTicks provider_end_time = base::TimeTicks::Now();
    std::string name = std::string("Omnibox.ProviderTime.") + (*i)->GetName();
    base::HistogramBase* counter = base::Histogram::FactoryGet(
        name, 1, 5000, 20, base::Histogram::kUmaTargetedHistogramFlag);
    counter->Add(static_cast<int>(
        (provider_end_time - provider_start_time).InMilliseconds()));
  }
  if (input.want_asynchronous_matches() && (input.text().length() < 6)) {
    base::TimeTicks end_time = base::TimeTicks::Now();
    std::string name = "Omnibox.QueryTime." + base::IntToString(
        input.text().length());
    base::HistogramBase* counter = base::Histogram::FactoryGet(
        name, 1, 1000, 50, base::Histogram::kUmaTargetedHistogramFlag);
    counter->Add(static_cast<int>((end_time - start_time).InMilliseconds()));
  }
  in_start_ = false;
  CheckIfDone();
  // The second true forces saying the default match has changed.
  // This triggers the edit model to update things such as the inline
  // autocomplete state.  In particular, if the user has typed a key
  // since the last notification, and we're now re-running
  // autocomplete, then we need to update the inline autocompletion
  // even if the current match is for the same URL as the last run's
  // default match.  Likewise, the controller doesn't know what's
  // happened in the edit since the last time it ran autocomplete.
  // The user might have selected all the text and hit delete, then
  // typed a new character.  The selection and delete won't send any
  // signals to the controller so it doesn't realize that anything was
  // cleared or changed.  Even if the default match hasn't changed, we
  // need the edit model to update the display.
  UpdateResult(false, true);

  if (!done_) {
    StartExpireTimer();
    StartStopTimer();
  }
}

void AutocompleteController::Stop(bool clear_result) {
  for (Providers::const_iterator i(providers_.begin()); i != providers_.end();
       ++i) {
    (*i)->Stop(clear_result);
  }

  expire_timer_.Stop();
  stop_timer_.Stop();
  done_ = true;
  if (clear_result && !result_.empty()) {
    result_.Reset();
    // NOTE: We pass in false since we're trying to only clear the popup, not
    // touch the edit... this is all a mess and should be cleaned up :(
    NotifyChanged(false);
  }
}

void AutocompleteController::StartZeroSuggest(const AutocompleteInput& input) {
  if (zero_suggest_provider_ == NULL)
    return;

  DCHECK(!in_start_);  // We should not be already running a query.

  // Call Start() on all prefix-based providers with an INVALID
  // AutocompleteInput to clear out cached |matches_|, which ensures that
  // they aren't used with zero suggest.
  for (Providers::iterator i(providers_.begin()); i != providers_.end(); ++i) {
    if (*i == zero_suggest_provider_)
      (*i)->Start(input, false);
    else
      (*i)->Start(AutocompleteInput(), false);
  }

  if (!zero_suggest_provider_->matches().empty())
    UpdateResult(false, false);
}

void AutocompleteController::DeleteMatch(const AutocompleteMatch& match) {
  DCHECK(match.SupportsDeletion());

  // Delete duplicate matches attached to the main match first.
  for (ACMatches::const_iterator it(match.duplicate_matches.begin());
       it != match.duplicate_matches.end(); ++it) {
    if (it->deletable)
      it->provider->DeleteMatch(*it);
  }

  if (match.deletable)
    match.provider->DeleteMatch(match);

  OnProviderUpdate(true);

  // If we're not done, we might attempt to redisplay the deleted match. Make
  // sure we aren't displaying it by removing any old entries.
  ExpireCopiedEntries();
}

void AutocompleteController::ExpireCopiedEntries() {
  // The first true makes UpdateResult() clear out the results and
  // regenerate them, thus ensuring that no results from the previous
  // result set remain.
  UpdateResult(true, false);
}

void AutocompleteController::OnProviderUpdate(bool updated_matches) {
  CheckIfDone();
  // Multiple providers may provide synchronous results, so we only update the
  // results if we're not in Start().
  if (!in_start_ && (updated_matches || done_))
    UpdateResult(false, false);
}

void AutocompleteController::AddProvidersInfo(
    ProvidersInfo* provider_info) const {
  provider_info->clear();
  for (Providers::const_iterator i(providers_.begin()); i != providers_.end();
       ++i) {
    // Add per-provider info, if any.
    (*i)->AddProviderInfo(provider_info);

    // This is also a good place to put code to add info that you want to
    // add for every provider.
  }
}

void AutocompleteController::ResetSession() {
  for (Providers::const_iterator i(providers_.begin()); i != providers_.end();
       ++i)
    (*i)->ResetSession();
}

void AutocompleteController::UpdateMatchDestinationURLWithQueryFormulationTime(
    base::TimeDelta query_formulation_time,
    AutocompleteMatch* match) const {
  if (!match->search_terms_args.get() ||
      match->search_terms_args->assisted_query_stats.empty())
    return;

  // Append the query formulation time (time from when the user first typed a
  // character into the omnibox to when the user selected a query) and whether
  // a field trial has triggered to the AQS parameter.
  TemplateURLRef::SearchTermsArgs search_terms_args(*match->search_terms_args);
  search_terms_args.assisted_query_stats += base::StringPrintf(
      ".%" PRId64 "j%dj%d",
      query_formulation_time.InMilliseconds(),
      (search_provider_ &&
       search_provider_->field_trial_triggered_in_session()) ||
      (zero_suggest_provider_ &&
       zero_suggest_provider_->field_trial_triggered_in_session()),
      input_.current_page_classification());
  UpdateMatchDestinationURL(search_terms_args, match);
}

void AutocompleteController::UpdateMatchDestinationURL(
    const TemplateURLRef::SearchTermsArgs& search_terms_args,
    AutocompleteMatch* match) const {
  TemplateURL* template_url = match->GetTemplateURL(
      template_url_service_, false);
  if (!template_url)
    return;

  match->destination_url = GURL(template_url->url_ref().ReplaceSearchTerms(
      search_terms_args, template_url_service_->search_terms_data()));
}

void AutocompleteController::UpdateResult(
    bool regenerate_result,
    bool force_notify_default_match_changed) {
  const bool last_default_was_valid = result_.default_match() != result_.end();
  // The following three variables are only set and used if
  // |last_default_was_valid|.
  base::string16 last_default_fill_into_edit, last_default_keyword,
      last_default_associated_keyword;
  if (last_default_was_valid) {
    last_default_fill_into_edit = result_.default_match()->fill_into_edit;
    last_default_keyword = result_.default_match()->keyword;
    if (result_.default_match()->associated_keyword != NULL)
      last_default_associated_keyword =
          result_.default_match()->associated_keyword->keyword;
  }

  if (regenerate_result)
    result_.Reset();

  AutocompleteResult last_result;
  last_result.Swap(&result_);

  for (Providers::const_iterator i(providers_.begin());
       i != providers_.end(); ++i)
    result_.AppendMatches((*i)->matches());

  // Sort the matches and trim to a small number of "best" matches.
  result_.SortAndCull(input_, template_url_service_);

  // Need to validate before invoking CopyOldMatches as the old matches are not
  // valid against the current input.
#ifndef NDEBUG
  result_.Validate();
#endif

  if (!done_) {
    // This conditional needs to match the conditional in Start that invokes
    // StartExpireTimer.
    result_.CopyOldMatches(input_, last_result, template_url_service_);
  }

  UpdateKeywordDescriptions(&result_);
  UpdateAssociatedKeywords(&result_);
  UpdateAssistedQueryStats(&result_);
  if (search_provider_)
    search_provider_->RegisterDisplayedAnswers(result_);

  const bool default_is_valid = result_.default_match() != result_.end();
  base::string16 default_associated_keyword;
  if (default_is_valid &&
      (result_.default_match()->associated_keyword != NULL)) {
    default_associated_keyword =
        result_.default_match()->associated_keyword->keyword;
  }
  // We've gotten async results. Send notification that the default match
  // updated if fill_into_edit, associated_keyword, or keyword differ.  (The
  // second can change if we've just started Chrome and the keyword database
  // finishes loading while processing this request.  The third can change
  // if we swapped from interpreting the input as a search--which gets
  // labeled with the default search provider's keyword--to a URL.)
  // We don't check the URL as that may change for the default match
  // even though the fill into edit hasn't changed (see SearchProvider
  // for one case of this).
  const bool notify_default_match =
      (last_default_was_valid != default_is_valid) ||
      (last_default_was_valid &&
       ((result_.default_match()->fill_into_edit !=
          last_default_fill_into_edit) ||
        (default_associated_keyword != last_default_associated_keyword) ||
        (result_.default_match()->keyword != last_default_keyword)));
  if (notify_default_match)
    last_time_default_match_changed_ = base::TimeTicks::Now();

  NotifyChanged(force_notify_default_match_changed || notify_default_match);
}

void AutocompleteController::UpdateAssociatedKeywords(
    AutocompleteResult* result) {
  if (!keyword_provider_)
    return;

  std::set<base::string16> keywords;
  for (ACMatches::iterator match(result->begin()); match != result->end();
       ++match) {
    base::string16 keyword(
        match->GetSubstitutingExplicitlyInvokedKeyword(template_url_service_));
    if (!keyword.empty()) {
      keywords.insert(keyword);
      continue;
    }

    // Only add the keyword if the match does not have a duplicate keyword with
    // a more relevant match.
    keyword = match->associated_keyword.get() ?
        match->associated_keyword->keyword :
        keyword_provider_->GetKeywordForText(match->fill_into_edit);
    if (!keyword.empty() && !keywords.count(keyword)) {
      keywords.insert(keyword);

      if (!match->associated_keyword.get())
        match->associated_keyword.reset(new AutocompleteMatch(
            keyword_provider_->CreateVerbatimMatch(match->fill_into_edit,
                                                   keyword, input_)));
    } else {
      match->associated_keyword.reset();
    }
  }
}

void AutocompleteController::UpdateKeywordDescriptions(
    AutocompleteResult* result) {
  base::string16 last_keyword;
  for (AutocompleteResult::iterator i(result->begin()); i != result->end();
       ++i) {
    if (AutocompleteMatch::IsSearchType(i->type)) {
      if (AutocompleteMatchHasCustomDescription(*i))
        continue;
      i->description.clear();
      i->description_class.clear();
      DCHECK(!i->keyword.empty());
      if (i->keyword != last_keyword) {
        const TemplateURL* template_url =
            i->GetTemplateURL(template_url_service_, false);
        if (template_url) {
          // For extension keywords, just make the description the extension
          // name -- don't assume that the normal search keyword description is
          // applicable.
          i->description = template_url->AdjustedShortNameForLocaleDirection();
          if (template_url->GetType() != TemplateURL::OMNIBOX_API_EXTENSION) {
            i->description = l10n_util::GetStringFUTF16(
                IDS_AUTOCOMPLETE_SEARCH_DESCRIPTION, i->description);
          }
          i->description_class.push_back(
              ACMatchClassification(0, ACMatchClassification::DIM));
        }
        last_keyword = i->keyword;
      }
    } else {
      last_keyword.clear();
    }
  }
}

void AutocompleteController::UpdateAssistedQueryStats(
    AutocompleteResult* result) {
  if (result->empty())
    return;

  // Build the impressions string (the AQS part after ".").
  std::string autocompletions;
  int count = 0;
  size_t last_type = base::string16::npos;
  size_t last_subtype = base::string16::npos;
  for (ACMatches::iterator match(result->begin()); match != result->end();
       ++match) {
    size_t type = base::string16::npos;
    size_t subtype = base::string16::npos;
    AutocompleteMatchToAssistedQuery(
        match->type, match->provider, &type, &subtype);
    if (last_type != base::string16::npos &&
        (type != last_type || subtype != last_subtype)) {
      AppendAvailableAutocompletion(
          last_type, last_subtype, count, &autocompletions);
      count = 1;
    } else {
      count++;
    }
    last_type = type;
    last_subtype = subtype;
  }
  AppendAvailableAutocompletion(
      last_type, last_subtype, count, &autocompletions);
  // Go over all matches and set AQS if the match supports it.
  for (size_t index = 0; index < result->size(); ++index) {
    AutocompleteMatch* match = result->match_at(index);
    const TemplateURL* template_url =
        match->GetTemplateURL(template_url_service_, false);
    if (!template_url || !match->search_terms_args.get())
      continue;
    std::string selected_index;
    // Prevent trivial suggestions from getting credit for being selected.
    if (!IsTrivialAutocompletion(*match))
      selected_index = base::StringPrintf("%" PRIuS, index);
    match->search_terms_args->assisted_query_stats =
        base::StringPrintf("chrome.%s.%s",
                           selected_index.c_str(),
                           autocompletions.c_str());
    match->destination_url = GURL(template_url->url_ref().ReplaceSearchTerms(
        *match->search_terms_args, template_url_service_->search_terms_data()));
  }
}

void AutocompleteController::NotifyChanged(bool notify_default_match) {
  if (delegate_)
    delegate_->OnResultChanged(notify_default_match);
  if (done_) {
    content::NotificationService::current()->Notify(
        chrome::NOTIFICATION_AUTOCOMPLETE_CONTROLLER_RESULT_READY,
        content::Source<AutocompleteController>(this),
        content::NotificationService::NoDetails());
  }
}

void AutocompleteController::CheckIfDone() {
  for (Providers::const_iterator i(providers_.begin()); i != providers_.end();
       ++i) {
    if (!(*i)->done()) {
      done_ = false;
      return;
    }
  }
  done_ = true;
}

void AutocompleteController::StartExpireTimer() {
  // Amount of time (in ms) between when the user stops typing and
  // when we remove any copied entries. We do this from the time the
  // user stopped typing as some providers (such as SearchProvider)
  // wait for the user to stop typing before they initiate a query.
  const int kExpireTimeMS = 500;

  if (result_.HasCopiedMatches())
    expire_timer_.Start(FROM_HERE,
                        base::TimeDelta::FromMilliseconds(kExpireTimeMS),
                        this, &AutocompleteController::ExpireCopiedEntries);
}

void AutocompleteController::StartStopTimer() {
  stop_timer_.Start(FROM_HERE,
                    stop_timer_duration_,
                    base::Bind(&AutocompleteController::Stop,
                               base::Unretained(this),
                               false));
}
