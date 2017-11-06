// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/omnibox_metrics_provider.h"

#include <vector>

#include "base/bind.h"
#include "base/logging.h"
#include "base/strings/string16.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "components/metrics/metrics_log.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_provider.h"
#include "components/omnibox/browser/autocomplete_result.h"
#include "components/omnibox/browser/omnibox_log.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"
#include "third_party/metrics_proto/omnibox_input_type.pb.h"

using metrics::OmniboxEventProto;

namespace {

OmniboxEventProto::Suggestion::ResultType AsOmniboxEventResultType(
    AutocompleteMatch::Type type) {
  switch (type) {
    case AutocompleteMatchType::URL_WHAT_YOU_TYPED:
      return OmniboxEventProto::Suggestion::URL_WHAT_YOU_TYPED;
    case AutocompleteMatchType::HISTORY_URL:
      return OmniboxEventProto::Suggestion::HISTORY_URL;
    case AutocompleteMatchType::HISTORY_TITLE:
      return OmniboxEventProto::Suggestion::HISTORY_TITLE;
    case AutocompleteMatchType::HISTORY_BODY:
      return OmniboxEventProto::Suggestion::HISTORY_BODY;
    case AutocompleteMatchType::HISTORY_KEYWORD:
      return OmniboxEventProto::Suggestion::HISTORY_KEYWORD;
    case AutocompleteMatchType::NAVSUGGEST:
      return OmniboxEventProto::Suggestion::NAVSUGGEST;
    case AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED:
      return OmniboxEventProto::Suggestion::SEARCH_WHAT_YOU_TYPED;
    case AutocompleteMatchType::SEARCH_HISTORY:
      return OmniboxEventProto::Suggestion::SEARCH_HISTORY;
    case AutocompleteMatchType::SEARCH_SUGGEST:
      return OmniboxEventProto::Suggestion::SEARCH_SUGGEST;
    case AutocompleteMatchType::SEARCH_SUGGEST_ENTITY:
      return OmniboxEventProto::Suggestion::SEARCH_SUGGEST_ENTITY;
    case AutocompleteMatchType::SEARCH_SUGGEST_TAIL:
      return OmniboxEventProto::Suggestion::SEARCH_SUGGEST_TAIL;
    case AutocompleteMatchType::SEARCH_SUGGEST_PERSONALIZED:
      return OmniboxEventProto::Suggestion::SEARCH_SUGGEST_PERSONALIZED;
    case AutocompleteMatchType::SEARCH_SUGGEST_PROFILE:
      return OmniboxEventProto::Suggestion::SEARCH_SUGGEST_PROFILE;
    case AutocompleteMatchType::CALCULATOR:
      return OmniboxEventProto::Suggestion::CALCULATOR;
    case AutocompleteMatchType::SEARCH_OTHER_ENGINE:
      return OmniboxEventProto::Suggestion::SEARCH_OTHER_ENGINE;
    case AutocompleteMatchType::EXTENSION_APP:
      return OmniboxEventProto::Suggestion::EXTENSION_APP;
    case AutocompleteMatchType::BOOKMARK_TITLE:
      return OmniboxEventProto::Suggestion::BOOKMARK_TITLE;
    case AutocompleteMatchType::NAVSUGGEST_PERSONALIZED:
      return OmniboxEventProto::Suggestion::NAVSUGGEST_PERSONALIZED;
    case AutocompleteMatchType::CLIPBOARD:
      return OmniboxEventProto::Suggestion::CLIPBOARD;
    case AutocompleteMatchType::PHYSICAL_WEB:
      return OmniboxEventProto::Suggestion::PHYSICAL_WEB;
    case AutocompleteMatchType::PHYSICAL_WEB_OVERFLOW:
      return OmniboxEventProto::Suggestion::PHYSICAL_WEB_OVERFLOW;
    case AutocompleteMatchType::TAB_SEARCH:
      // TODO(crbug.com/46672): Create a specific type and move this result
      // under it.
      return OmniboxEventProto::Suggestion::UNKNOWN_RESULT_TYPE;
    case AutocompleteMatchType::VOICE_SUGGEST:
      // VOICE_SUGGEST matches are only used in Java and are not logged,
      // so we should never reach this case.
    case AutocompleteMatchType::CONTACT_DEPRECATED:
    case AutocompleteMatchType::NUM_TYPES:
      break;
  }
  NOTREACHED();
  return OmniboxEventProto::Suggestion::UNKNOWN_RESULT_TYPE;
}

}  // namespace

OmniboxMetricsProvider::OmniboxMetricsProvider(
    const base::Callback<bool(void)>& is_off_the_record_callback)
    : is_off_the_record_callback_(is_off_the_record_callback) {}

OmniboxMetricsProvider::~OmniboxMetricsProvider() {
}

void OmniboxMetricsProvider::OnRecordingEnabled() {
  subscription_ = OmniboxEventGlobalTracker::GetInstance()->RegisterCallback(
      base::Bind(&OmniboxMetricsProvider::OnURLOpenedFromOmnibox,
                 base::Unretained(this)));
}

void OmniboxMetricsProvider::OnRecordingDisabled() {
  subscription_.reset();
}

void OmniboxMetricsProvider::ProvideCurrentSessionData(
    metrics::ChromeUserMetricsExtension* uma_proto) {
  uma_proto->mutable_omnibox_event()->Swap(
      omnibox_events_cache.mutable_omnibox_event());
}

void OmniboxMetricsProvider::OnURLOpenedFromOmnibox(OmniboxLog* log) {
  // Do not log events to UMA if the embedder reports that the user is in an
  // off-the-record context.
  if (!is_off_the_record_callback_.Run())
    RecordOmniboxOpenedURL(*log);
}

void OmniboxMetricsProvider::RecordOmniboxOpenedURL(const OmniboxLog& log) {
  std::vector<base::StringPiece16> terms = base::SplitStringPiece(
      log.text, base::kWhitespaceUTF16,
      base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  OmniboxEventProto* omnibox_event = omnibox_events_cache.add_omnibox_event();
  omnibox_event->set_time_sec(metrics::MetricsLog::GetCurrentTime());
  if (log.tab_id != -1) {
    // If we know what tab the autocomplete URL was opened in, log it.
    omnibox_event->set_tab_id(log.tab_id);
  }
  omnibox_event->set_typed_length(log.text.length());
  omnibox_event->set_just_deleted_text(log.just_deleted_text);
  omnibox_event->set_num_typed_terms(static_cast<int>(terms.size()));
  omnibox_event->set_selected_index(log.selected_index);
  if (log.completed_length != base::string16::npos)
    omnibox_event->set_completed_length(log.completed_length);
  const base::TimeDelta default_time_delta =
      base::TimeDelta::FromMilliseconds(-1);
  if (log.elapsed_time_since_user_first_modified_omnibox !=
      default_time_delta) {
    // Only upload the typing duration if it is set/valid.
    omnibox_event->set_typing_duration_ms(
        log.elapsed_time_since_user_first_modified_omnibox.InMilliseconds());
  }
  if (log.elapsed_time_since_last_change_to_default_match !=
      default_time_delta) {
    omnibox_event->set_duration_since_last_default_match_update_ms(
        log.elapsed_time_since_last_change_to_default_match.InMilliseconds());
  }
  omnibox_event->set_current_page_classification(
      log.current_page_classification);
  omnibox_event->set_input_type(log.input_type);
  // We consider a paste-and-search/paste-and-go action to have a closed popup
  // (as explained in omnibox_event.proto) even if it was not, because such
  // actions ignore the contents of the popup so it doesn't matter that it was
  // open.
  omnibox_event->set_is_popup_open(log.is_popup_open && !log.is_paste_and_go);
  omnibox_event->set_is_paste_and_go(log.is_paste_and_go);

  for (AutocompleteResult::const_iterator i(log.result.begin());
       i != log.result.end(); ++i) {
    OmniboxEventProto::Suggestion* suggestion = omnibox_event->add_suggestion();
    const auto provider_type = i->provider->AsOmniboxEventProviderType();
    suggestion->set_provider(provider_type);
    suggestion->set_result_type(AsOmniboxEventResultType(i->type));
    suggestion->set_relevance(i->relevance);
    if (i->typed_count != -1)
      suggestion->set_typed_count(i->typed_count);
    if (i->subtype_identifier > 0)
      suggestion->set_result_subtype_identifier(i->subtype_identifier);
  }
  for (ProvidersInfo::const_iterator i(log.providers_info.begin());
       i != log.providers_info.end(); ++i) {
    OmniboxEventProto::ProviderInfo* provider_info =
        omnibox_event->add_provider_info();
    provider_info->CopyFrom(*i);
  }
}
