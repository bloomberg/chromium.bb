// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/omnibox_metrics_provider.h"

#include <vector>

#include "base/logging.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/omnibox/omnibox_log.h"
#include "chrome/browser/ui/browser_otr_state.h"
#include "components/metrics/metrics_log.h"
#include "components/metrics/proto/omnibox_event.pb.h"
#include "components/metrics/proto/omnibox_input_type.pb.h"
#include "components/omnibox/autocomplete_match.h"
#include "components/omnibox/autocomplete_provider.h"
#include "components/omnibox/autocomplete_result.h"
#include "content/public/browser/notification_service.h"

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
    case AutocompleteMatchType::SEARCH_SUGGEST_INFINITE:
      return OmniboxEventProto::Suggestion::SEARCH_SUGGEST_INFINITE;
    case AutocompleteMatchType::SEARCH_SUGGEST_PERSONALIZED:
      return OmniboxEventProto::Suggestion::SEARCH_SUGGEST_PERSONALIZED;
    case AutocompleteMatchType::SEARCH_SUGGEST_PROFILE:
      return OmniboxEventProto::Suggestion::SEARCH_SUGGEST_PROFILE;
    case AutocompleteMatchType::SEARCH_SUGGEST_ANSWER:
      return OmniboxEventProto::Suggestion::SEARCH_SUGGEST_ANSWER;
    case AutocompleteMatchType::SEARCH_OTHER_ENGINE:
      return OmniboxEventProto::Suggestion::SEARCH_OTHER_ENGINE;
    case AutocompleteMatchType::EXTENSION_APP:
      return OmniboxEventProto::Suggestion::EXTENSION_APP;
    case AutocompleteMatchType::BOOKMARK_TITLE:
      return OmniboxEventProto::Suggestion::BOOKMARK_TITLE;
    case AutocompleteMatchType::NAVSUGGEST_PERSONALIZED:
      return OmniboxEventProto::Suggestion::NAVSUGGEST_PERSONALIZED;
    case AutocompleteMatchType::CONTACT_DEPRECATED:
    case AutocompleteMatchType::NUM_TYPES:
      break;
  }
  NOTREACHED();
  return OmniboxEventProto::Suggestion::UNKNOWN_RESULT_TYPE;
}

}  // namespace

OmniboxMetricsProvider::OmniboxMetricsProvider() {
}

OmniboxMetricsProvider::~OmniboxMetricsProvider() {
}

void OmniboxMetricsProvider::OnRecordingEnabled() {
  registrar_.Add(this, chrome::NOTIFICATION_OMNIBOX_OPENED_URL,
                 content::NotificationService::AllSources());
}

void OmniboxMetricsProvider::OnRecordingDisabled() {
  registrar_.RemoveAll();
}

void OmniboxMetricsProvider::ProvideGeneralMetrics(
    metrics::ChromeUserMetricsExtension* uma_proto) {
  uma_proto->mutable_omnibox_event()->Swap(
      omnibox_events_cache.mutable_omnibox_event());
}

void OmniboxMetricsProvider::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_EQ(chrome::NOTIFICATION_OMNIBOX_OPENED_URL, type);

  // We simply don't log events to UMA if there is a single incognito
  // session visible. In the future, it may be worth revisiting this to
  // still log events from non-incognito sessions.
  if (!chrome::IsOffTheRecordSessionActive())
    RecordOmniboxOpenedURL(*content::Details<OmniboxLog>(details).ptr());
}

void OmniboxMetricsProvider::RecordOmniboxOpenedURL(const OmniboxLog& log) {
  std::vector<base::string16> terms;
  const int num_terms =
      static_cast<int>(Tokenize(log.text, base::kWhitespaceUTF16, &terms));

  OmniboxEventProto* omnibox_event = omnibox_events_cache.add_omnibox_event();
  omnibox_event->set_time(metrics::MetricsLog::GetCurrentTime());
  if (log.tab_id != -1) {
    // If we know what tab the autocomplete URL was opened in, log it.
    omnibox_event->set_tab_id(log.tab_id);
  }
  omnibox_event->set_typed_length(log.text.length());
  omnibox_event->set_just_deleted_text(log.just_deleted_text);
  omnibox_event->set_num_typed_terms(num_terms);
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
  const bool consider_popup_open = log.is_popup_open && !log.is_paste_and_go;
  omnibox_event->set_is_popup_open(consider_popup_open);
  omnibox_event->set_is_paste_and_go(log.is_paste_and_go);
  if (consider_popup_open) {
    omnibox_event->set_is_top_result_hidden_in_dropdown(
        log.result.ShouldHideTopMatch());
  }

  for (AutocompleteResult::const_iterator i(log.result.begin());
       i != log.result.end(); ++i) {
    OmniboxEventProto::Suggestion* suggestion = omnibox_event->add_suggestion();
    suggestion->set_provider(i->provider->AsOmniboxEventProviderType());
    suggestion->set_result_type(AsOmniboxEventResultType(i->type));
    suggestion->set_relevance(i->relevance);
    if (i->typed_count != -1)
      suggestion->set_typed_count(i->typed_count);
  }
  for (ProvidersInfo::const_iterator i(log.providers_info.begin());
       i != log.providers_info.end(); ++i) {
    OmniboxEventProto::ProviderInfo* provider_info =
        omnibox_event->add_provider_info();
    provider_info->CopyFrom(*i);
  }
}
