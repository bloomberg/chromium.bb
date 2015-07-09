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
#include "chrome/browser/autocomplete/chrome_autocomplete_provider_client.h"
#include "chrome/browser/autocomplete/chrome_autocomplete_scheme_classifier.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/url_database.h"
#include "components/metrics/proto/omnibox_event.pb.h"
#include "components/omnibox/browser/autocomplete_classifier.h"
#include "components/omnibox/browser/autocomplete_controller.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_provider.h"
#include "components/search_engines/template_url.h"
#include "content/public/browser/web_ui.h"
#include "mojo/common/common_type_converters.h"

using bookmarks::BookmarkModel;

namespace mojo {

template <>
struct TypeConverter<mojo::Array<AutocompleteAdditionalInfoPtr>,
                     AutocompleteMatch::AdditionalInfo> {
  static mojo::Array<AutocompleteAdditionalInfoPtr> Convert(
      const AutocompleteMatch::AdditionalInfo& input) {
    mojo::Array<AutocompleteAdditionalInfoPtr> array(input.size());
    size_t index = 0;
    for (AutocompleteMatch::AdditionalInfo::const_iterator i = input.begin();
         i != input.end(); ++i, index++) {
      AutocompleteAdditionalInfoPtr item(AutocompleteAdditionalInfo::New());
      item->key = i->first;
      item->value = i->second;
      array[index] = item.Pass();
    }
    return array.Pass();
  }
};

template <>
struct TypeConverter<AutocompleteMatchMojoPtr, AutocompleteMatch> {
  static AutocompleteMatchMojoPtr Convert(const AutocompleteMatch& input) {
    AutocompleteMatchMojoPtr result(AutocompleteMatchMojo::New());
    if (input.provider != NULL) {
      result->provider_name = input.provider->GetName();
      result->provider_done = input.provider->done();
    }
    result->relevance = input.relevance;
    result->deletable = input.deletable;
    result->fill_into_edit = mojo::String::From(input.fill_into_edit);
    result->inline_autocompletion =
        mojo::String::From(input.inline_autocompletion);
    result->destination_url = input.destination_url.spec();
    result->contents = mojo::String::From(input.contents);
    // At this time, we're not bothering to send along the long vector that
    // represent contents classification.  i.e., for each character, what
    // type of text it is.
    result->description = mojo::String::From(input.description);
    // At this time, we're not bothering to send along the long vector that
    // represents description classification.  i.e., for each character, what
    // type of text it is.
    result->transition = input.transition;
    result->allowed_to_be_default_match = input.allowed_to_be_default_match;
    result->type = AutocompleteMatchType::ToString(input.type);
    if (input.associated_keyword.get() != NULL) {
      result->associated_keyword =
          mojo::String::From(input.associated_keyword->keyword);
    }
    result->keyword = mojo::String::From(input.keyword);
    result->duplicates = static_cast<int32>(input.duplicate_matches.size());
    result->from_previous = input.from_previous;

    result->additional_info =
        mojo::Array<AutocompleteAdditionalInfoPtr>::From(input.additional_info);
    return result.Pass();
  }
};

template <>
struct TypeConverter<AutocompleteResultsForProviderMojoPtr,
                     scoped_refptr<AutocompleteProvider> > {
  static AutocompleteResultsForProviderMojoPtr Convert(
      const scoped_refptr<AutocompleteProvider>& input) {
    AutocompleteResultsForProviderMojoPtr result(
        AutocompleteResultsForProviderMojo::New());
    result->provider_name = input->GetName();
    result->results =
        mojo::Array<AutocompleteMatchMojoPtr>::From(input->matches());
    return result.Pass();
  }
};

}  // namespace mojo

OmniboxUIHandler::OmniboxUIHandler(
    Profile* profile,
    mojo::InterfaceRequest<OmniboxUIHandlerMojo> request)
    : profile_(profile),
      binding_(this, request.Pass()) {
  ResetController();
}

OmniboxUIHandler::~OmniboxUIHandler() {
}

void OmniboxUIHandler::OnResultChanged(bool default_match_changed) {
  OmniboxResultMojoPtr result(OmniboxResultMojo::New());
  result->done = controller_->done();
  result->time_since_omnibox_started_ms =
      (base::Time::Now() - time_omnibox_started_).InMilliseconds();
  const base::string16 host = input_.text().substr(
      input_.parts().host.begin,
      input_.parts().host.len);
  result->host = mojo::String::From(host);
  bool is_typed_host;
  if (!LookupIsTypedHost(host, &is_typed_host))
    is_typed_host = false;
  result->is_typed_host = is_typed_host;

  {
    // Copy to an ACMatches to make conversion easier. Since this isn't
    // performance critical we don't worry about the cost here.
    ACMatches matches(controller_->result().begin(),
                      controller_->result().end());
    result->combined_results =
        mojo::Array<AutocompleteMatchMojoPtr>::From(matches);
  }
  result->results_by_provider =
      mojo::Array<AutocompleteResultsForProviderMojoPtr>::From(
          controller_->providers());

  // Fill AutocompleteMatchMojo::starred.
  BookmarkModel* bookmark_model = BookmarkModelFactory::GetForProfile(profile_);
  if (bookmark_model) {
    for (size_t i =  0; i < result->combined_results.size(); ++i) {
      result->combined_results[i]->starred = bookmark_model->IsBookmarked(
          GURL(result->combined_results[i]->destination_url));
    }
    for (size_t i = 0; i < result->results_by_provider.size(); ++i) {
      const AutocompleteResultsForProviderMojo& result_by_provider =
          *result->results_by_provider[i];
      for (size_t j = 0; j < result_by_provider.results.size(); ++j) {
        result_by_provider.results[j]->starred = bookmark_model->IsBookmarked(
            GURL(result_by_provider.results[j]->destination_url));
      }
    }
  }

  page_->HandleNewAutocompleteResult(result.Pass());
}

bool OmniboxUIHandler::LookupIsTypedHost(const base::string16& host,
                                         bool* is_typed_host) const {
  history::HistoryService* const history_service =
      HistoryServiceFactory::GetForProfile(profile_,
                                           ServiceAccessType::EXPLICIT_ACCESS);
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
                                         int32_t page_classification,
                                         OmniboxPagePtr page) {
  // Reset the controller.  If we don't do this, then the
  // AutocompleteController might inappropriately set its |minimal_changes|
  // variable (or something else) and some providers will short-circuit
  // important logic and return stale results.  In short, we want the
  // actual results to not depend on the state of the previous request.
  ResetController();
  page_ = page.Pass();
  time_omnibox_started_ = base::Time::Now();
  input_ = AutocompleteInput(
      input_string.To<base::string16>(), cursor_position, std::string(), GURL(),
      static_cast<metrics::OmniboxEventProto::PageClassification>(
          page_classification),
      prevent_inline_autocomplete, prefer_keyword, true, true, false,
      ChromeAutocompleteSchemeClassifier(profile_));
  controller_->Start(input_);
}

void OmniboxUIHandler::ResetController() {
  controller_.reset(new AutocompleteController(
      make_scoped_ptr(new ChromeAutocompleteProviderClient(profile_)), this,
      AutocompleteClassifier::kDefaultOmniboxProviders));
}
