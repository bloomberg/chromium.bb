// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/omnibox/omnibox_ui_handler.h"

#include <string>

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/strings/string16.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/autocomplete/autocomplete_classifier.h"
#include "chrome/browser/autocomplete/autocomplete_controller.h"
#include "chrome/browser/autocomplete/autocomplete_input.h"
#include "chrome/browser/autocomplete/autocomplete_match.h"
#include "chrome/browser/autocomplete/autocomplete_provider.h"
#include "chrome/browser/history/history_service.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/history/url_database.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/search_engines/template_url.h"
#include "content/public/browser/web_ui.h"
#include "mojo/common/common_type_converters.h"
#include "mojo/public/cpp/bindings/allocation_scope.h"

namespace mojo {

template <>
class TypeConverter<mojo::Array<AutocompleteAdditionalInfo>,
                    AutocompleteMatch::AdditionalInfo> {
 public:
  static mojo::Array<AutocompleteAdditionalInfo> ConvertFrom(
      const AutocompleteMatch::AdditionalInfo& input,
      Buffer* buf) {
    mojo::Array<AutocompleteAdditionalInfo>::Builder array_builder(
        input.size(), buf);
    size_t index = 0;
    for (AutocompleteMatch::AdditionalInfo::const_iterator i = input.begin();
         i != input.end(); ++i, index++) {
      AutocompleteAdditionalInfo::Builder item_builder(buf);
      item_builder.set_key(i->first);
      item_builder.set_value(i->second);
      array_builder[index] = item_builder.Finish();
    }
    return array_builder.Finish();
  }
};

template <>
class TypeConverter<AutocompleteMatchMojo, AutocompleteMatch> {
 public:
  static AutocompleteMatchMojo ConvertFrom(const AutocompleteMatch& input,
                                           Buffer* buf) {
    AutocompleteMatchMojo::Builder builder(buf);
    if (input.provider != NULL) {
      builder.set_provider_name(input.provider->GetName());
      builder.set_provider_done(input.provider->done());
    }
    builder.set_relevance(input.relevance);
    builder.set_deletable(input.deletable);
    builder.set_fill_into_edit(input.fill_into_edit);
    builder.set_inline_autocompletion(input.inline_autocompletion);
    builder.set_destination_url(input.destination_url.spec());
    builder.set_contents(input.contents);
    // At this time, we're not bothering to send along the long vector that
    // represent contents classification.  i.e., for each character, what
    // type of text it is.
    builder.set_description(input.description);
    // At this time, we're not bothering to send along the long vector that
    // represents description classification.  i.e., for each character, what
    // type of text it is.
    builder.set_transition(input.transition);
    builder.set_is_history_what_you_typed_match(
        input.is_history_what_you_typed_match);
    builder.set_allowed_to_be_default_match(input.allowed_to_be_default_match);
    builder.set_type(AutocompleteMatchType::ToString(input.type));
    if (input.associated_keyword.get() != NULL)
      builder.set_associated_keyword(input.associated_keyword->keyword);
    builder.set_keyword(input.keyword);
    builder.set_starred(input.starred);
    builder.set_duplicates(static_cast<int32>(input.duplicate_matches.size()));
    builder.set_from_previous(input.from_previous);

    builder.set_additional_info(input.additional_info);
    return builder.Finish();
  }
};

template <>
class TypeConverter<AutocompleteResultsForProviderMojo, AutocompleteProvider*> {
 public:
  static AutocompleteResultsForProviderMojo ConvertFrom(
      const AutocompleteProvider* input,
      Buffer* buf) {
    AutocompleteResultsForProviderMojo::Builder builder(buf);
    builder.set_provider_name(input->GetName());
    builder.set_results(input->matches());
    return builder.Finish();
  }
};

}  // namespace mojo

OmniboxUIHandler::OmniboxUIHandler(ScopedOmniboxPageHandle handle,
                                   Profile* profile)
    : page_(handle.Pass(), this),
      profile_(profile) {
  ResetController();
}

OmniboxUIHandler::~OmniboxUIHandler() {}

void OmniboxUIHandler::OnResultChanged(bool default_match_changed) {
  mojo::AllocationScope scope;
  OmniboxResultMojo::Builder builder;
  builder.set_done(controller_->done());
  builder.set_time_since_omnibox_started_ms(
      (base::Time::Now() - time_omnibox_started_).InMilliseconds());
  const base::string16 host = controller_->input().text().substr(
      controller_->input().parts().host.begin,
      controller_->input().parts().host.len);
  builder.set_host(host);
  bool is_typed_host;
  if (!LookupIsTypedHost(host, &is_typed_host))
    is_typed_host = false;
  builder.set_is_typed_host(is_typed_host);

  {
    // Copy to an ACMatches to make conversion easier. Since this isn't
    // performance critical we don't worry about the cost here.
    ACMatches matches(controller_->result().begin(),
                      controller_->result().end());
    builder.set_combined_results(matches);
  }
  builder.set_results_by_provider(*controller_->providers());
  page_->HandleNewAutocompleteResult(builder.Finish());
}

bool OmniboxUIHandler::LookupIsTypedHost(const base::string16& host,
                                         bool* is_typed_host) const {
  HistoryService* const history_service =
      HistoryServiceFactory::GetForProfile(profile_,
                                           Profile::EXPLICIT_ACCESS);
  if (!history_service)
    return false;
  history::URLDatabase* url_db = history_service->InMemoryDatabase();
  if (!url_db)
    return false;
  *is_typed_host = url_db->IsTypedHost(base::UTF16ToUTF8(host));
  return true;
}

void OmniboxUIHandler::StartOmniboxQuery(const mojo::String& input_string,
                                         int32_t cursor_position,
                                         bool prevent_inline_autocomplete,
                                         bool prefer_keyword,
                                         int32_t page_classification) {
  // Reset the controller.  If we don't do this, then the
  // AutocompleteController might inappropriately set its |minimal_changes|
  // variable (or something else) and some providers will short-circuit
  // important logic and return stale results.  In short, we want the
  // actual results to not depend on the state of the previous request.
  ResetController();
  time_omnibox_started_ = base::Time::Now();
  controller_->Start(AutocompleteInput(
      input_string,
      cursor_position,
      base::string16(),  // user's desired tld (top-level domain)
      GURL(),
      static_cast<AutocompleteInput::PageClassification>(
          page_classification),
      prevent_inline_autocomplete,
      prefer_keyword,
      true,  // allow exact keyword matches
      AutocompleteInput::ALL_MATCHES));  // want all matches
}

void OmniboxUIHandler::ResetController() {
  controller_.reset(new AutocompleteController(profile_, this,
          AutocompleteClassifier::kDefaultOmniboxProviders));
}
