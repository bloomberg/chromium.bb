// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/omnibox_pedal.h"

#include "base/strings/utf_string_conversions.h"
#include "components/omnibox/browser/omnibox_client.h"
#include "components/omnibox/browser/omnibox_edit_controller.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

OmniboxPedal::LabelStrings::LabelStrings(int id_hint,
                                         int id_hint_short,
                                         int id_suggestion_contents)
    : hint(l10n_util::GetStringUTF16(id_hint)),
      hint_short(l10n_util::GetStringUTF16(id_hint_short)),
      suggestion_contents(l10n_util::GetStringUTF16(id_suggestion_contents)) {}

OmniboxPedal::OmniboxPedal(OmniboxPedal::LabelStrings strings)
    : strings_(strings) {}

OmniboxPedal::~OmniboxPedal() {}

const OmniboxPedal::LabelStrings& OmniboxPedal::GetLabelStrings() const {
  return strings_;
}

bool OmniboxPedal::IsNavigation() const {
  return !url_.is_empty();
}

const GURL& OmniboxPedal::GetNavigationUrl() const {
  return url_;
}

bool OmniboxPedal::ShouldExecute(bool button_pressed) const {
  const auto mode = OmniboxFieldTrial::GetPedalSuggestionMode();
  return (mode == OmniboxFieldTrial::PedalSuggestionMode::DEDICATED) ||
         (mode == OmniboxFieldTrial::PedalSuggestionMode::IN_SUGGESTION &&
          button_pressed);
}

bool OmniboxPedal::ShouldPresentButton() const {
  return OmniboxFieldTrial::GetPedalSuggestionMode() ==
         OmniboxFieldTrial::PedalSuggestionMode::IN_SUGGESTION;
}

void OmniboxPedal::Execute(OmniboxPedal::ExecutionContext& context) const {
  DCHECK(IsNavigation());
  OpenURL(context, url_);
}

bool OmniboxPedal::IsTriggerMatch(const base::string16& match_text) const {
  return triggers_.find(match_text) != triggers_.end();
}

void OmniboxPedal::OpenURL(OmniboxPedal::ExecutionContext& context,
                           const GURL& url) const {
  context.controller_.OnAutocompleteAccept(
      url, WindowOpenDisposition::CURRENT_TAB, ui::PAGE_TRANSITION_GENERATED,
      AutocompleteMatchType::PEDAL, context.match_selection_timestamp_);
}

// =============================================================================

OmniboxPedalClearBrowsingData::OmniboxPedalClearBrowsingData()
    : OmniboxPedal(OmniboxPedal::LabelStrings(
          IDS_OMNIBOX_PEDAL_CLEAR_BROWSING_DATA_HINT,
          IDS_OMNIBOX_PEDAL_CLEAR_BROWSING_DATA_HINT_SHORT,
          IDS_OMNIBOX_PEDAL_CLEAR_BROWSING_DATA_SUGGESTION_CONTENTS)) {
  url_ = GURL("chrome://settings/clearBrowserData");
  // TODO(orinj): Move all trigger strings to files (maybe even per language).
  const auto triggers = {
      "how to clear browsing data on chrome",
      "how to clear history",
      "how to clear history on google chrome",
      "how to clear history on chrome",
      "how to clear history in google chrome",
      "how to clear google chrome history",
      "how to clear history google chrome",
      "how to clear browsing history in chrome",
      "clear browsing data",
      "clear history",
      "clear browsing data on chrome",
      "clear history on google chrome",
      "clear history google chrome",
      "clear browsing history in chrome",
      "clear cookies chrome",
      "clear chrome history",
      "clear chrome cache",
      "history clear",
      "history clear chrome",
  };

  for (const auto* trigger : triggers) {
    triggers_.insert(base::ASCIIToUTF16(trigger));
  }
}

// =============================================================================

OmniboxPedalChangeSearchEngine::OmniboxPedalChangeSearchEngine()
    : OmniboxPedal(OmniboxPedal::LabelStrings(
          IDS_OMNIBOX_PEDAL_CHANGE_SEARCH_ENGINE_HINT,
          IDS_OMNIBOX_PEDAL_CHANGE_SEARCH_ENGINE_HINT_SHORT,
          IDS_OMNIBOX_PEDAL_CHANGE_SEARCH_ENGINE_SUGGESTION_CONTENTS)) {
  url_ = GURL("chrome://settings/searchEngines");
  const auto triggers = {
      "how to change search engine",
      "how to change default search engine",
      "how to change default search engine in chrome",
      "how to change search engine on chrome",
      "how to set google as default search engine on chrome",
      "how to set google as default search engine in chrome",
      "how to make google default search engine",
      "how to change default search engine in google chrome",
      "change search engine",
      "change google search engine",
      "change chrome searh engine",
      "change default search engine in chrome",
      "change search engine chrome",
      "change default search chrome",
      "change search chrome",
      "switch chrome search engine",
      "switch search engine",
  };

  for (const auto* trigger : triggers) {
    triggers_.insert(base::ASCIIToUTF16(trigger));
  }
}

// =============================================================================

OmniboxPedalManagePasswords::OmniboxPedalManagePasswords()
    : OmniboxPedal(OmniboxPedal::LabelStrings(
          IDS_OMNIBOX_PEDAL_MANAGE_PASSWORDS_HINT,
          IDS_OMNIBOX_PEDAL_MANAGE_PASSWORDS_HINT_SHORT,
          IDS_OMNIBOX_PEDAL_MANAGE_PASSWORDS_SUGGESTION_CONTENTS)) {
  url_ = GURL("chrome://settings/passwords");
  const auto triggers = {
      "passwords",
      "find my passwords",
      "save passwords in chrome",
      "view saved passwords",
      "delete passwords",
      "find saved passwords",
      "where does chrome store passwords",
      "how to see passwords in chrome",
  };

  for (const auto* trigger : triggers) {
    triggers_.insert(base::ASCIIToUTF16(trigger));
  }
}

// =============================================================================

OmniboxPedalChangeHomePage::OmniboxPedalChangeHomePage()
    : OmniboxPedal(OmniboxPedal::LabelStrings(
          IDS_OMNIBOX_PEDAL_CHANGE_HOME_PAGE_HINT,
          IDS_OMNIBOX_PEDAL_CHANGE_HOME_PAGE_HINT_SHORT,
          IDS_OMNIBOX_PEDAL_CHANGE_HOME_PAGE_SUGGESTION_CONTENTS)) {
  // TODO(orinj): Use better scoping for existing setting, or link to a new UI.
  url_ = GURL("chrome://settings/?search=show+home+button");
  const auto triggers = {
      "how to change home page",
      "how to change your home page",
      "how do i change my home page",
      "change home page google",
      "home page chrome",
      "change home chrome",
      "change chrome home page",
      "how to change home page on chrome",
      "how to change home page in chrome",
      "change chrome home",
  };

  for (const auto* trigger : triggers) {
    triggers_.insert(base::ASCIIToUTF16(trigger));
  }
}

// =============================================================================

OmniboxPedalUpdateCreditCard::OmniboxPedalUpdateCreditCard()
    : OmniboxPedal(OmniboxPedal::LabelStrings(
          IDS_OMNIBOX_PEDAL_UPDATE_CREDIT_CARD_HINT,
          IDS_OMNIBOX_PEDAL_UPDATE_CREDIT_CARD_HINT_SHORT,
          IDS_OMNIBOX_PEDAL_UPDATE_CREDIT_CARD_SUGGESTION_CONTENTS)) {
  url_ = GURL("chrome://settings/autofill");
  const auto triggers = {
      "how to save credit card info on chrome",
      "how to remove credit card from google chrome",
      "remove google chrome credit cards",
      "access google chrome credit cards",
      "google chrome credit cards",
      "chrome credit cards",
      "get to chrome credit cards",
      "chrome credit saved",
  };

  for (const auto* trigger : triggers) {
    triggers_.insert(base::ASCIIToUTF16(trigger));
  }
}
