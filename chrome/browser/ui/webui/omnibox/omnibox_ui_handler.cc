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
#include "chrome/browser/autocomplete/autocomplete.h"
#include "chrome/browser/autocomplete/autocomplete_match.h"
#include "chrome/browser/search_engines/template_url.h"
#include "content/public/browser/web_ui.h"

OmniboxUIHandler::OmniboxUIHandler(Profile* profile ) {
  controller_.reset(new AutocompleteController(profile, this));
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
// { 'done': false,
//   'time_since_omnibox_started_ms': 15,
//   'num_items': 4,
//   'item_0': {
//     'destination_url': 'http://mail.google.com',
//     'provider_name': 'HistoryURL',
//     'relevance': 1410,
//     'is_default_match': true,
//     ...
//    }
//   'item_1: {
//     ...
//   }
//   ...
// }
// For reference, the javascript code that unpacks this object and
// displays it is in chrome/browser/resources/omnibox.js
//
// TODO(mpearson): Instead of iterating over the sorted-and-
// duplicate-eliminated results, perhaps it would be useful to also
// poll all the AutocompleteController's providers.  This will require
// changing the AutocompleteController's interface.
void OmniboxUIHandler::OnResultChanged(bool default_match_changed) {
  const AutocompleteResult& result = controller_->result();
  base::DictionaryValue result_to_output;
  // Fill in result-level information.
  result_to_output.SetBoolean("done", controller_->done());
  result_to_output.SetInteger("time_since_omnibox_started_ms",
      (base::Time::Now() - time_omnibox_started_).InMilliseconds());
  result_to_output.SetInteger("num_items", result.size());
  // Fill in per-match information.
  int i = 0;
  for (ACMatches::const_iterator it = result.begin(); it != result.end();
       ++it, ++i) {
    std::string prefix(StringPrintf("item_%d", i));
    if (it->provider != NULL) {
      result_to_output.SetString(prefix + ".provider_name",
                                 it->provider->name());
      result_to_output.SetBoolean(prefix + ".provider_done",
                                  it->provider->done());
    }
    result_to_output.SetInteger(prefix + ".relevance", it->relevance);
    result_to_output.SetBoolean(prefix + ".deletable", it->deletable);
    result_to_output.SetString(prefix + ".fill_into_edit", it->fill_into_edit);
    result_to_output.SetInteger(prefix + ".inline_autocomplete_offset",
                                it->inline_autocomplete_offset);
    result_to_output.SetString(prefix + ".destination_url",
                               it->destination_url.spec());
    result_to_output.SetString(prefix + ".contents", it->contents);
    // At this time, we're not bothering to send along the long vector that
    // represent contents classification.  i.e., for each character, what
    // type of text it is.
    result_to_output.SetString(prefix + ".description", it->description);
    // At this time, we're not bothering to send along the long vector that
    // represents description classification.  i.e., for each character, what
    // type of text it is.
    result_to_output.SetInteger(prefix + ".transition", it->transition);
    result_to_output.SetBoolean(prefix + ".is_history_what_you_typed_match",
                                it->is_history_what_you_typed_match);
    result_to_output.SetString(prefix + ".type",
                               AutocompleteMatch::TypeToString(it->type));
    if ((it->template_url != NULL) && (it->template_url->url() != NULL)) {
      result_to_output.SetString(prefix + ".template_url",
                                 it->template_url->url()->url());
    }
    result_to_output.SetBoolean(prefix + ".starred", it->starred);
    result_to_output.SetBoolean(prefix + ".from_previous", it->from_previous);
    result_to_output.SetBoolean(prefix + ".is_default_match",
                                it == result.default_match());
  }
  web_ui()->CallJavascriptFunction("omniboxDebug.handleNewAutocompleteResult",
                                  result_to_output);
}

void OmniboxUIHandler::StartOmniboxQuery(
    const base::ListValue* one_element_input_string) {
  string16 input_string = ExtractStringValue(one_element_input_string);
  string16 empty_string;
  // Tell the autocomplete controller to start working on the
  // input.  It's okay if the previous request hasn't yet finished;
  // the autocomplete controller is smart enough to stop the previous
  // query before it starts the new one.  By the way, in this call to
  // Start(), we use the default/typical values for all parameters.
  time_omnibox_started_ = base::Time::Now();
  controller_->Start(input_string,
                     empty_string,  // user's desired tld (top-level domain)
                     false,  // don't prevent inline autocompletion
                     false,  // no preferred keyword provider
                     true,  // allow exact keyword matches
                     AutocompleteInput::ALL_MATCHES);  // want all matches
}
