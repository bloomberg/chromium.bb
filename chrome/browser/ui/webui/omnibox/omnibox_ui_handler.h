// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OMNIBOX_OMNIBOX_UI_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_OMNIBOX_OMNIBOX_UI_HANDLER_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/time.h"
#include "chrome/browser/autocomplete/autocomplete_controller_delegate.h"
#include "chrome/browser/autocomplete/autocomplete_match.h"
#include "content/public/browser/web_ui_message_handler.h"

class AutocompleteController;
class Profile;

namespace base {
class ListValue;
}

// UI Handler for chrome://omnibox/
// It listens for calls from javascript to StartOmniboxQuery() and
// passes those calls to its private AutocompleteController. It also
// listens for updates from the AutocompleteController to OnResultChanged()
// and passes those results on calling back into the Javascript to
// update the page.
class OmniboxUIHandler : public AutocompleteControllerDelegate,
                         public content::WebUIMessageHandler {
 public:
  explicit OmniboxUIHandler(Profile* profile);
  virtual ~OmniboxUIHandler();

  // AutocompleteControllerDelegate implementation.
  // Gets called when the result set of the AutocompleteController changes.
  // We transform the AutocompleteResult into a Javascript object and
  // call the Javascript function gotNewAutocompleteResult with it.
  // |default_match_changed| is given to us by the AutocompleteController
  // but we don't need it.  It's ignored.
  virtual void OnResultChanged(bool default_match_changed) OVERRIDE;

 protected:
  // WebUIMessageHandler implementation.
  // Register our handler to get callbacks from javascript for
  // startOmniboxQuery().
  virtual void RegisterMessages() OVERRIDE;

 private:
  // Gets called from the javascript when a user enters text into the
  // chrome://omnibox/ text box and clicks submit or hits enter.
  // |two_element_input_string| is expected to be a two-element list
  // where the first element is the input string and the second element
  // is a boolean indicating whether we should set prevent_inline_autocomplete
  // or not.
  void StartOmniboxQuery(const base::ListValue* two_element_input_string);

  // Helper function for OnResultChanged().
  // Takes an iterator over AutocompleteMatches and packages them into
  // the DictionaryValue output, all stored under the given prefix.
  void AddResultToDictionary(const std::string& prefix,
                             ACMatches::const_iterator result_it,
                             ACMatches::const_iterator end,
                             base::DictionaryValue* output);

  // The omnibox AutocompleteController that collects/sorts/dup-
  // eliminates the results as they come in.
  scoped_ptr<AutocompleteController> controller_;

  // Time the user's input was sent to the omnibox to start searching.
  // Needed because we also pass timing information in the object we
  // hand back to the javascript.
  base::Time time_omnibox_started_;

  DISALLOW_COPY_AND_ASSIGN(OmniboxUIHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_OMNIBOX_OMNIBOX_UI_HANDLER_H_
