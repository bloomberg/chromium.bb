// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/omnibox_pedal.h"

#include <cctype>

#include "base/strings/utf_string_conversions.h"
#include "components/omnibox/browser/omnibox_client.h"
#include "components/omnibox/browser/omnibox_edit_controller.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "ui/base/l10n/l10n_util.h"

#if (!defined(OS_ANDROID) || BUILDFLAG(ENABLE_VR)) && !defined(OS_IOS)
#include "components/omnibox/browser/vector_icons.h"  // nogncheck
#endif

OmniboxPedal::LabelStrings::LabelStrings(int id_hint,
                                         int id_hint_short,
                                         int id_suggestion_contents)
    : hint(l10n_util::GetStringUTF16(id_hint)),
      hint_short(l10n_util::GetStringUTF16(id_hint_short)),
      suggestion_contents(l10n_util::GetStringUTF16(id_suggestion_contents)) {}

// =============================================================================

OmniboxPedal::SynonymGroup::SynonymGroup(
    bool required,
    std::initializer_list<const char*> synonyms)
    : required_(required) {
  // The DCHECK logic below is quickly ensuring that the synonyms are provided
  // in descending order of string length.
#if DCHECK_IS_ON()
  size_t min_size = std::numeric_limits<std::size_t>::max();
#endif
  synonyms_.reserve(synonyms.size());
  for (const char* synonym : synonyms) {
    synonyms_.push_back(base::ASCIIToUTF16(synonym));
#if DCHECK_IS_ON()
    size_t size = synonyms_.back().size();
    DCHECK_LE(size, min_size);
    min_size = size;
#endif
  }
}

OmniboxPedal::SynonymGroup::SynonymGroup(const SynonymGroup& other) = default;

OmniboxPedal::SynonymGroup::~SynonymGroup() = default;

bool OmniboxPedal::SynonymGroup::EraseFirstMatchIn(
    base::string16& remaining) const {
  for (const auto& synonym : synonyms_) {
    const size_t pos = remaining.find(synonym);
    if (pos != base::string16::npos) {
      remaining.erase(pos, synonym.size());
      return true;
    }
  }
  return !required_;
}

// =============================================================================

OmniboxPedal::OmniboxPedal(
    LabelStrings strings,
    GURL url,
    std::initializer_list<const char*> triggers,
    std::initializer_list<const SynonymGroup> synonym_groups)
    : strings_(strings), url_(url) {
  triggers_.reserve(triggers.size());
  for (const char* trigger : triggers) {
    triggers_.insert(base::ASCIIToUTF16(trigger));
  }
  synonym_groups_.reserve(synonym_groups.size());
  for (const SynonymGroup& group : synonym_groups) {
    synonym_groups_.push_back(group);
  }
}

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

void OmniboxPedal::Execute(OmniboxPedal::ExecutionContext& context) const {
  DCHECK(IsNavigation());
  OpenURL(context, url_);
}

bool OmniboxPedal::IsReadyToTrigger(
    const AutocompleteProviderClient& client) const {
  return true;
}

#if (!defined(OS_ANDROID) || BUILDFLAG(ENABLE_VR)) && !defined(OS_IOS)
const gfx::VectorIcon& OmniboxPedal::GetVectorIcon() const {
  return omnibox::kPedalIcon;
}
#endif

bool OmniboxPedal::IsTriggerMatch(const base::string16& match_text) const {
  return (triggers_.find(match_text) != triggers_.end()) ||
         IsConceptMatch(match_text);
}

bool OmniboxPedal::IsConceptMatch(const base::string16& match_text) const {
  base::string16 remaining = match_text;
  for (const auto& group : synonym_groups_) {
    if (!group.EraseFirstMatchIn(remaining))
      return false;
  }
  // If any non-space is remaining, it means there is something in match_text
  // that was not covered by groups, so conservatively treat it as non-match.
  const auto is_space = [](auto c) { return std::isspace(c); };
  return std::all_of(remaining.begin(), remaining.end(), is_space);
}

void OmniboxPedal::OpenURL(OmniboxPedal::ExecutionContext& context,
                           const GURL& url) const {
  context.controller_.OnAutocompleteAccept(
      url, nullptr, WindowOpenDisposition::CURRENT_TAB,
      ui::PAGE_TRANSITION_GENERATED, AutocompleteMatchType::PEDAL,
      context.match_selection_timestamp_);
}
