// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ntp/suggestions_source_discovery.h"

#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/json/json_reader.h"
#include "base/stl_util.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/api/discovery/suggested_link.h"
#include "chrome/browser/extensions/api/discovery/suggested_links_registry.h"
#include "chrome/browser/extensions/api/discovery/suggested_links_registry_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/ntp/new_tab_ui.h"
#include "chrome/browser/ui/webui/ntp/suggestions_combiner.h"
#include "net/base/load_flags.h"

namespace {

typedef extensions::SuggestedLinksRegistry::SuggestedLinkList SuggestedLinkList;

// The weight used by the combiner to determine which ratio of suggestions
// should be obtained from this source.
const int kSuggestionsDiscoveryWeight = 1;

}  // namespace

SuggestionsSourceDiscovery::SuggestionsSourceDiscovery(
    const std::string& extension_id)
    : combiner_(NULL),
      extension_id_(extension_id),
      debug_(false) {}

SuggestionsSourceDiscovery::~SuggestionsSourceDiscovery() {
  STLDeleteElements(&items_);
}

void SuggestionsSourceDiscovery::SetDebug(bool enable) {
  debug_ = enable;
}

inline int SuggestionsSourceDiscovery::GetWeight() {
  return kSuggestionsDiscoveryWeight;
}

int SuggestionsSourceDiscovery::GetItemCount() {
  return items_.size();
}

DictionaryValue* SuggestionsSourceDiscovery::PopItem() {
  if (items_.empty())
    return NULL;
  DictionaryValue* item = items_.front();
  items_.pop_front();
  return item;
}

void SuggestionsSourceDiscovery::FetchItems(Profile* profile) {
  DCHECK(combiner_);

  extensions::SuggestedLinksRegistry* registry =
      extensions::SuggestedLinksRegistryFactory::GetForProfile(profile);
  const SuggestedLinkList* list = registry->GetAll(extension_id_);

  items_.clear();
  for (SuggestedLinkList::const_iterator it = list->begin();
       it != list->end(); ++it) {
    DictionaryValue* page_value = new DictionaryValue();
    NewTabUI::SetURLTitleAndDirection(page_value,
                                      ASCIIToUTF16((*it)->link_text()),
                                      GURL((*it)->link_url()));
    page_value->SetDouble("score", (*it)->score());
    items_.push_back(page_value);
  }
  combiner_->OnItemsReady();
}

void SuggestionsSourceDiscovery::SetCombiner(SuggestionsCombiner* combiner) {
  DCHECK(!combiner_);
  combiner_ = combiner;
}
