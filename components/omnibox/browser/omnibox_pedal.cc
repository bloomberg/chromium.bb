// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/omnibox_pedal.h"

#include <cctype>

#include "base/strings/utf_string_conversions.h"
#include "components/omnibox/browser/omnibox_client.h"
#include "components/omnibox/browser/omnibox_edit_controller.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "third_party/re2/src/re2/re2.h"
#include "ui/base/l10n/l10n_util.h"

#if (!defined(OS_ANDROID) || BUILDFLAG(ENABLE_VR)) && !defined(OS_IOS)
#include "components/omnibox/browser/vector_icons.h"  // nogncheck
#endif

namespace {
// Erases one or more instances of whole words from |text|. Removable words are
// provided in |words| and must match in |text| either at a string boundary or
// next to whitespace in order to be removed.  If |erase_once| is true, then at
// most a single erasure will take place (the rightmost occurrence of first word
// found).  Returns true if erasure occurred; false if |text| remained
// unchanged. Note that a 'word' may actually be a phrase with inner whitespace,
// but there should be no whitespace at either end of any word.
bool EraseWords(base::string16& text,
                const std::vector<base::string16>& words,
                bool erase_once) {
  std::string text_utf8 = base::UTF16ToUTF8(text);
  if (text_utf8.find("\\E") != std::string::npos) {
    // Do not process strings containing regex quote end.
    return false;
  }
  bool changed = false;
  for (const auto& word : words) {
    std::string search_expression("(^|\\s)\\Q");
    search_expression += base::UTF16ToUTF8(word);
    search_expression += "\\E($|\\s)";
    RE2 regex(search_expression);
    // GlobalReplace doesn't work here because it only handles nonoverlapping
    // occurrences, but the spaces may cause overlaps; so we Replace in a loop.
    for (;;) {
      // The replacement includes two spaces because the match has max two, so
      // using just one could bring initially-separated words together.
      const bool replaced = RE2::Replace(&text_utf8, regex, "  ");
      changed |= replaced;
      if (!replaced || erase_once) {
        break;
      }
    }
    if (changed && erase_once) {
      break;
    }
  }
  if (changed) {
    text = base::UTF8ToUTF16(text_utf8);
  }
  return changed;
}

}  // namespace

OmniboxPedal::LabelStrings::LabelStrings(int id_hint,
                                         int id_hint_short,
                                         int id_suggestion_contents)
    : hint(l10n_util::GetStringUTF16(id_hint)),
      hint_short(l10n_util::GetStringUTF16(id_hint_short)),
      suggestion_contents(l10n_util::GetStringUTF16(id_suggestion_contents)) {}

// =============================================================================

OmniboxPedal::SynonymGroup::SynonymGroup(bool required, bool match_once)
    : required_(required), match_once_(match_once) {}

OmniboxPedal::SynonymGroup::SynonymGroup(const SynonymGroup& other) = default;

OmniboxPedal::SynonymGroup::~SynonymGroup() = default;

bool OmniboxPedal::SynonymGroup::EraseMatchesIn(
    base::string16& remaining) const {
  const bool changed = EraseWords(remaining, synonyms_, match_once_);
  return changed || !required_;
}

void OmniboxPedal::SynonymGroup::AddSynonym(const base::string16& synonym) {
#if DCHECK_IS_ON()
  if (synonyms_.size() > size_t{0}) {
    DCHECK_GE(synonyms_.back().length(), synonym.length());
  }
#endif
  synonyms_.push_back(synonym);
}

// =============================================================================

OmniboxPedal::OmniboxPedal(LabelStrings strings,
                           GURL url,
                           std::initializer_list<const char*> triggers)
    : strings_(strings), url_(url) {
  triggers_.reserve(triggers.size());
  for (const char* trigger : triggers) {
    triggers_.insert(base::ASCIIToUTF16(trigger));
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

void OmniboxPedal::AddSynonymGroup(const SynonymGroup& group) {
  synonym_groups_.push_back(group);
}

bool OmniboxPedal::IsConceptMatch(const base::string16& match_text) const {
  base::string16 remaining = match_text;
  for (const auto& group : synonym_groups_) {
    if (!group.EraseMatchesIn(remaining))
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
