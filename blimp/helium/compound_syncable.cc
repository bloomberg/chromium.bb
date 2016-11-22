// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/helium/compound_syncable.h"

#include <algorithm>
#include <bitset>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "third_party/protobuf/src/google/protobuf/io/coded_stream.h"

namespace blimp {
namespace helium {

CompoundChangeset::CompoundChangeset()
    : compound_changesets(this, std::map<int, std::string>()) {}

CompoundChangeset::~CompoundChangeset() {}

CompoundSyncable::CompoundSyncable() {}

CompoundSyncable::~CompoundSyncable() {}

std::unique_ptr<CompoundChangeset> CompoundSyncable::CreateChangeset(
    Revision from) const {
  // Populate a map with the serialized changesets of modified member
  // Syncables.
  std::map<int, std::string> compound_changesets_map;
  for (size_t i = 0; i < members_.size(); ++i) {
    if (members_[i]->GetRevision() >= from) {
      std::string changeset_data;
      google::protobuf::io::StringOutputStream raw_output_stream(
          &changeset_data);
      google::protobuf::io::CodedOutputStream output_stream(&raw_output_stream);
      members_[i]->CreateChangeset(from, &output_stream);
      output_stream.Trim();
      if (!changeset_data.empty()) {
        compound_changesets_map[i] = std::move(changeset_data);
      }
    }
  }

  std::unique_ptr<CompoundChangeset> output_changeset =
      base::MakeUnique<CompoundChangeset>();
  output_changeset->compound_changesets.Set(std::move(compound_changesets_map));
  return output_changeset;
}

void CompoundSyncable::ApplyChangeset(const CompoundChangeset& changeset) {
  for (const auto& current_changeset : changeset.compound_changesets()) {
    members_[current_changeset.first]->ApplyChangeset();
  }
}

Revision CompoundSyncable::GetRevision() const {
  Revision merged_revision = {};
  for (const auto& member : members_) {
    merged_revision = std::max(merged_revision, member->GetRevision());
  }
  return merged_revision;
}

void CompoundSyncable::SetLocalUpdateCallback(
    const base::Closure& local_update_callback) {
  for (auto* member : members_) {
    member->SetLocalUpdateCallback(local_update_callback);
  }
}

void CompoundSyncable::ReleaseBefore(Revision from) {
  for (auto* member : members_) {
    member->ReleaseBefore(from);
  }
}

bool CompoundSyncable::ValidateChangeset(
    const CompoundChangeset& changeset) const {
  for (const auto& current_changeset : changeset.compound_changesets()) {
    int32_t syncable_id = current_changeset.first;
    auto changeset = current_changeset.second;
    if (syncable_id >= static_cast<int>(members_.size())) {
      return false;
    }

    google::protobuf::io::ArrayInputStream raw_input_stream(changeset.data(),
                                                            changeset.size());
    google::protobuf::io::CodedInputStream input_stream(&raw_input_stream);

    if (!members_[syncable_id]->ParseAndValidate(&input_stream)) {
      return false;
    }
  }

  return true;
}

}  // namespace helium
}  // namespace blimp
