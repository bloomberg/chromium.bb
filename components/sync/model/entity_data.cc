// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/model/entity_data.h"

#include <algorithm>
#include <utility>

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "components/sync/base/time.h"
#include "components/sync/base/unique_position.h"
#include "components/sync/protocol/proto_value_conversions.h"

namespace syncer {

namespace {

std::string UniquePositionToString(
    const sync_pb::UniquePosition& unique_position) {
  return UniquePosition::FromProto(unique_position).ToDebugString();
}

}  // namespace

EntityData::EntityData() {}
EntityData::~EntityData() {}

void EntityData::Swap(EntityData* other) {
  id.swap(other->id);
  client_tag_hash.swap(other->client_tag_hash);
  non_unique_name.swap(other->non_unique_name);

  specifics.Swap(&other->specifics);

  std::swap(creation_time, other->creation_time);
  std::swap(modification_time, other->modification_time);

  parent_id.swap(other->parent_id);
  unique_position.Swap(&other->unique_position);
}

EntityDataPtr EntityData::PassToPtr() {
  EntityDataPtr target;
  target.swap_value(this);
  return target;
}

#define ADD_TO_DICT(dict, value, transform) \
  dict->SetString(base::ToUpperASCII(#value), transform(value));

std::unique_ptr<base::DictionaryValue> EntityData::ToDictionaryValue() {
  std::unique_ptr<base::DictionaryValue> dict =
      EntitySpecificsToValue(specifics);
  ADD_TO_DICT(dict, id, );
  ADD_TO_DICT(dict, client_tag_hash, );
  ADD_TO_DICT(dict, non_unique_name, );
  ADD_TO_DICT(dict, parent_id, );
  ADD_TO_DICT(dict, creation_time, GetTimeDebugString);
  ADD_TO_DICT(dict, modification_time, GetTimeDebugString);
  ADD_TO_DICT(dict, unique_position, UniquePositionToString);
  return dict;
}

#undef ADD_TO_DICT

void EntityDataTraits::SwapValue(EntityData* dest, EntityData* src) {
  dest->Swap(src);
}

bool EntityDataTraits::HasValue(const EntityData& value) {
  return !value.client_tag_hash.empty();
}

const EntityData& EntityDataTraits::DefaultValue() {
  CR_DEFINE_STATIC_LOCAL(EntityData, default_instance, ());
  return default_instance;
}

}  // namespace syncer
