// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/omnibox/omnibox_ui_handler.h"

#include <string>

#include "base/bind.h"
#include "base/string16.h"
#include "base/stringprintf.h"
#include "base/time.h"
#include "base/values.h"
#include "chrome/browser/autocomplete/autocomplete_classifier.h"
#include "chrome/browser/autocomplete/autocomplete_controller.h"
#include "chrome/browser/autocomplete/autocomplete_input.h"
#include "chrome/browser/autocomplete/autocomplete_match.h"
#include "chrome/browser/autocomplete/autocomplete_provider.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/ui/search/search.h"
#include "content/public/browser/web_ui.h"

OmniboxUIHandler::OmniboxUIHandler(Profile* profile ) {
  controller_.reset(new AutocompleteController(profile, this,
      chrome::search::IsInstantExtendedAPIEnabled(profile) ?
          AutocompleteClassifier::kInstantExtendedOmniboxProviders :
          AutocompleteClassifier::kDefaultOmniboxProviders));
}

OmniboxUIHandler::~OmniboxUIHandler() {}

void OmniboxUIHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback("startOmniboxQuery",
      base::Bind(&OmniboxUIHandler::StartOmniboxQuery,
                 base::Unretained(this)));
}

// This function gets called when the AutocompleteController possibly
// has new results.  We package those results in a DictionaryValue
// object result_to_output and call the javascript function
// handleNewAutocompleteResult.  Here's an example populated
// result_to_output object:
// {
//   'done': false,
//   'time_since_omnibox_started_ms': 15,
//   'combined_results' : {
//     'num_items': 4,
//     'item_0': {
//       'destination_url': 'http://mail.google.com',
//       'provider_name': 'HistoryURL',
//       'relevance': 1410,
//       ...
//     }
//     'item_1: {
//       ...
//     }
//     ...
//   }
//   'results_by_provider': {
//     'HistoryURL' : {
//       'num_items': 3,
//         ...
//       }
//     'Search' : {
//       'num_items': 1,
//       ...
//     }
//     ...
//   }
// }
// For reference, the javascript code that unpacks this object and
// displays it is in chrome/browser/resources/omnibox.js
void OmniboxUIHandler::OnResultChanged(bool default_match_changed) {
  base::DictionaryValue result_to_output;
  // Fill in general information.
  result_to_output.SetBoolean("done", controller_->done());
  result_to_output.SetInteger("time_since_omnibox_started_ms",
      (base::Time::Now() - time_omnibox_started_).InMilliseconds());
  // Fill in the merged/combined results the controller has provided.
  AddResultToDictionary("combined_results", controller_->result().begin(),
                        controller_->result().end(), &result_to_output);
  // Fill results from each individual provider as well.
  for (ACProviders::const_iterator it(controller_->providers()->begin());
       it != controller_->providers()->end(); ++it) {
    AddResultToDictionary(
        std::string("results_by_provider.") + (*it)->GetName(),
        (*it)->matches().begin(), (*it)->matches().end(), &result_to_output);
  }
  // Add done; send the results.
  web_ui()->CallJavascriptFunction("omniboxDebug.handleNewAutocompleteResult",
                                   result_to_output);
}

// For details on the format of the DictionaryValue that this function
// populates, see the comments by OnResultChanged().
void OmniboxUIHandler::AddResultToDictionary(const std::string& prefix,
                                             ACMatches::const_iterator it,
                                             ACMatches::const_iterator end,
                                             base::DictionaryValue* output) {
  int i = 0;
  for (; it != end; ++it, ++i) {
    std::string item_prefix(prefix + StringPrintf(".item_%d", i));
    if (it->provider != NULL) {
      output->SetString(item_prefix + ".provider_name",
                        it->provider->GetName());
      output->SetBoolean(item_prefix + ".provider_done", it->provider->done());
    }
    output->SetInteger(item_prefix + ".relevance", it->relevance);
    output->SetBoolean(item_prefix + ".deletable", it->deletable);
    output->SetString(item_prefix + ".fill_into_edit", it->fill_into_edit);
    output->SetInteger(item_prefix + ".inline_autocomplete_offset",
                       it->inline_autocomplete_offset);
    output->SetString(item_prefix + ".destination_url",
                      it->destination_url.spec());
    output->SetString(item_prefix + ".contents", it->contents);
    // At this time, we're not bothering to send along the long vector that
    // represent contents classification.  i.e., for each character, what
    // type of text it is.
    output->SetString(item_prefix + ".description", it->description);
    // At this time, we're not bothering to send along the long vector that
    // represents description classification.  i.e., for each character, what
    // type of text it is.
    output->SetInteger(item_prefix + ".transition", it->transition);
    output->SetBoolean(item_prefix + ".is_history_what_you_typed_match",
                       it->is_history_what_you_typed_match);
    output->SetString(item_prefix + ".type",
                      AutocompleteMatch::TypeToString(it->type));
    if (it->associated_keyword.get() != NULL) {
      output->SetString(item_prefix + ".associated_keyword",
                        it->associated_keyword->keyword);
    }
    output->SetString(item_prefix + ".keyword", it->keyword);
    output->SetBoolean(item_prefix + ".starred", it->starred);
    output->SetBoolean(item_prefix + ".from_previous", it->from_previous);
    for (AutocompleteMatch::AdditionalInfo::const_iterator j =
         it->additional_info.begin(); j != it->additional_info.end(); ++j) {
      output->SetString(item_prefix + ".additional_info." + j->first,
                        j->second);
    }
  }
  output->SetInteger(prefix + ".num_items", i);
}

void OmniboxUIHandler::StartOmniboxQuery(
    const base::ListValue* two_element_input_string) {
  DCHECK_EQ(2u, two_element_input_string->GetSize());
  string16 input_string;
  bool return_val = two_element_input_string->GetString(0, &input_string);
  DCHECK(return_val);
  bool prevent_inline_autocomplete;
  return_val =
      two_element_input_string->GetBoolean(1, &prevent_inline_autocomplete);
  DCHECK(return_val);
  string16 empty_string;
  // Tell the autocomplete controller to start working on the
  // input.  It's okay if the previous request hasn't yet finished;
  // the autocomplete controller is smart enough to stop the previous
  // query before it starts the new one.  By the way, in this call to
  // Start(), we use the default/typical values for all parameters.
  time_omnibox_started_ = base::Time::Now();
  controller_->Start(AutocompleteInput(
      input_string,
      string16::npos,
      empty_string,  // user's desired tld (top-level domain)
      prevent_inline_autocomplete,
      false,  // no preferred keyword provider
      true,  // allow exact keyword matches
      AutocompleteInput::ALL_MATCHES));  // want all matches
}
