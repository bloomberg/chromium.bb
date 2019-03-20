// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/omnibox_pedal_provider.h"

#include "base/i18n/case_conversion.h"
#include "base/json/json_reader.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/grit/components_resources.h"
#include "components/omnibox/browser/autocomplete_provider_client.h"
#include "components/omnibox/browser/omnibox_pedal.h"
#include "components/omnibox/browser/omnibox_pedal_implementations.h"
#include "third_party/zlib/google/compression_utils.h"
#include "ui/base/resource/resource_bundle.h"

OmniboxPedalProvider::OmniboxPedalProvider(AutocompleteProviderClient& client)
    : client_(client),
      pedals_(GetPedalImplementations()),
      ignore_group_(false, false) {
  LoadPedalConcepts();
}

OmniboxPedalProvider::~OmniboxPedalProvider() {}

OmniboxPedal* OmniboxPedalProvider::FindPedalMatch(
    const base::string16& match_text) const {
  base::string16 reduced_text = base::i18n::ToLower(match_text);
  ignore_group_.EraseMatchesIn(reduced_text);

  // Right now Pedals are few and small, but if this linear search ever
  // encounters performance concerns, see crrev.com/c/1247223 for a ready made
  // optimization that quickly eliminates the vast majority of searches.
  for (const auto& pedal : pedals_) {
    if (pedal.second->IsTriggerMatch(reduced_text) &&
        pedal.second->IsReadyToTrigger(client_)) {
      return pedal.second.get();
    }
  }
  return nullptr;
}

void OmniboxPedalProvider::LoadPedalConcepts() {
  // Get raw gzipped data, uncompress it, then parse to base::Value for loading.
  base::StringPiece compressed_data =
      ui::ResourceBundle::GetSharedInstance().GetRawDataResource(
          IDR_OMNIBOX_PEDAL_CONCEPTS);
  std::string uncompressed_data;
  uncompressed_data.resize(compression::GetUncompressedSize(compressed_data));
  CHECK(compression::GzipUncompress(compressed_data, uncompressed_data));
  const auto concept_data = base::JSONReader::Read(uncompressed_data);

  DCHECK(concept_data);
  DCHECK(concept_data->is_dict());

  const int data_version = concept_data->FindKey("data_version")->GetInt();
  CHECK_EQ(data_version, OMNIBOX_PEDAL_CONCEPTS_DATA_VERSION);

  const auto& dictionary = concept_data->FindKey("dictionary")->GetList();
  dictionary_.reserve(dictionary.size());
  for (const auto& token_value : dictionary) {
    base::string16 token;
    token_value.GetAsString(&token);
    dictionary_.push_back(token);
  }

  const base::Value* ignore_group_value = concept_data->FindKey("ignore_group");
  DCHECK_NE(ignore_group_value, nullptr);
  ignore_group_ = LoadSynonymGroup(*ignore_group_value);

  for (const auto& pedal_value : concept_data->FindKey("pedals")->GetList()) {
    DCHECK(pedal_value.is_dict());
    for (const auto& group_value : pedal_value.FindKey("groups")->GetList()) {
      OmniboxPedal::SynonymGroup synonym_group = LoadSynonymGroup(group_value);
      const OmniboxPedalId pedal_id =
          static_cast<OmniboxPedalId>(pedal_value.FindKey("id")->GetInt());
      const auto pedal = pedals_.find(pedal_id);
      if (pedal != pedals_.end()) {
        pedal->second->AddSynonymGroup(synonym_group);
      } else {
        DCHECK(false) << "OmniboxPedalId " << static_cast<int>(pedal_id)
                      << " not found. Are all data-referenced implementations "
                         "added to provider?";
      }
    }
  }
}

OmniboxPedal::SynonymGroup OmniboxPedalProvider::LoadSynonymGroup(
    const base::Value& group_value) const {
  DCHECK(group_value.is_dict());
  const bool required = group_value.FindKey("required")->GetBool();
  const bool single = group_value.FindKey("single")->GetBool();
  OmniboxPedal::SynonymGroup synonym_group(required, single);
  for (const auto& synonyms_value :
       group_value.FindKey("synonyms")->GetList()) {
    base::string16 synonym_all_tokens;
    for (const auto& token_index_value : synonyms_value.GetList()) {
      const size_t token_index = size_t{token_index_value.GetInt()};
      DCHECK_LT(token_index, dictionary_.size());
      synonym_all_tokens += dictionary_[token_index];
      synonym_all_tokens += L' ';
    }
    base::TrimWhitespace(synonym_all_tokens, base::TrimPositions::TRIM_ALL,
                         &synonym_all_tokens);
    synonym_group.AddSynonym(synonym_all_tokens);
  }
  return synonym_group;
}
