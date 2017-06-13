// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/test/fake_server/fake_server_entity.h"

#include <limits>
#include <memory>
#include <vector>

#include "base/guid.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "net/base/net_errors.h"
#include "net/http/http_status_code.h"

using std::string;
using std::vector;
using syncer::ModelType;

// The separator used when formatting IDs.
//
// We chose the underscore character because it doesn't conflict with the
// special characters used by base/base64.h's encoding, which is also used in
// the construction of some IDs.
const char kIdSeparator[] = "_";

namespace fake_server {

FakeServerEntity::~FakeServerEntity() {}

int64_t FakeServerEntity::GetVersion() const {
  return version_;
}

void FakeServerEntity::SetVersion(int64_t version) {
  version_ = version;
}

const std::string& FakeServerEntity::GetName() const {
  return name_;
}

void FakeServerEntity::SetName(const std::string& name) {
  name_ = name;
}

void FakeServerEntity::SetSpecifics(
    const sync_pb::EntitySpecifics& updated_specifics) {
  specifics_ = updated_specifics;
}

bool FakeServerEntity::IsDeleted() const {
  return false;
}

bool FakeServerEntity::IsFolder() const {
  return false;
}

bool FakeServerEntity::IsPermanent() const {
  return false;
}

// static
string FakeServerEntity::CreateId(const ModelType& model_type,
                                  const string& inner_id) {
  int field_number = GetSpecificsFieldNumberFromModelType(model_type);
  return base::StringPrintf("%d%s%s", field_number, kIdSeparator,
                            inner_id.c_str());
}

// static
std::string FakeServerEntity::GetTopLevelId(const ModelType& model_type) {
  return FakeServerEntity::CreateId(model_type,
                                    syncer::ModelTypeToRootTag(model_type));
}

// static
ModelType FakeServerEntity::GetModelTypeFromId(const string& id) {
  vector<base::StringPiece> tokens = base::SplitStringPiece(
      id, kIdSeparator, base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  int field_number;
  if (tokens.size() != 2 || !base::StringToInt(tokens[0], &field_number)) {
    return syncer::UNSPECIFIED;
  }

  return syncer::GetModelTypeFromSpecificsFieldNumber(field_number);
}

FakeServerEntity::FakeServerEntity(const string& id,
                                   const string& client_defined_unique_tag,
                                   const ModelType& model_type,
                                   int64_t version,
                                   const string& name)
    : id_(id),
      client_defined_unique_tag_(client_defined_unique_tag),
      model_type_(model_type),
      version_(version),
      name_(name) {
  // There shouldn't be a unique_tag if the type is bookmarks.
  DCHECK(model_type != syncer::BOOKMARKS || client_defined_unique_tag.empty());
}

void FakeServerEntity::SerializeBaseProtoFields(
    sync_pb::SyncEntity* sync_entity) const {
  sync_pb::EntitySpecifics* specifics = sync_entity->mutable_specifics();
  specifics->CopyFrom(specifics_);

  // FakeServerEntity fields
  sync_entity->set_id_string(id_);
  if (!client_defined_unique_tag_.empty())
    sync_entity->set_client_defined_unique_tag(client_defined_unique_tag_);
  sync_entity->set_version(version_);
  sync_entity->set_name(name_);

  // Data via accessors
  sync_entity->set_deleted(IsDeleted());
  sync_entity->set_folder(IsFolder());

  if (RequiresParentId())
    sync_entity->set_parent_id_string(GetParentId());
}

}  // namespace fake_server
