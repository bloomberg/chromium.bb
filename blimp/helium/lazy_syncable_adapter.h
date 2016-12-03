// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_HELIUM_LAZY_SYNCABLE_ADAPTER_H_
#define BLIMP_HELIUM_LAZY_SYNCABLE_ADAPTER_H_

#include <string>

#include "base/memory/ptr_util.h"
#include "blimp/helium/blimp_helium_export.h"
#include "blimp/helium/syncable.h"
#include "third_party/protobuf/src/google/protobuf/io/coded_stream.h"
#include "third_party/protobuf/src/google/protobuf/io/zero_copy_stream_impl_lite.h"

namespace blimp {
namespace helium {

// The LazySyncableAdapter is used to wrap LazySyncables of any changeset type
// into a LazySyncable with a string changeset by serializing the changesets
// into a string and parsing them from strings. That way, ObjectSyncState can
// just work with LazySyncables of type string.
template <class ChangesetType>
class BLIMP_HELIUM_EXPORT LazySyncableAdapter
    : public LazySyncable<std::string> {
 public:
  explicit LazySyncableAdapter(LazySyncable<ChangesetType>* inner_syncable)
      : inner_syncable_(inner_syncable) {}
  ~LazySyncableAdapter() = default;

  // LazySyncable implementation.
  std::unique_ptr<std::string> CreateChangeset(Revision from) const override;
  void ApplyChangeset(const std::string& changeset) override;
  bool ValidateChangeset(const std::string& changeset) const override;
  void SetLocalUpdateCallback(
      const base::Closure& local_update_callback) override;
  void ReleaseBefore(Revision before) override;
  Revision GetRevision() const override;
  void PrepareToCreateChangeset(Revision from, base::Closure done) override;

 private:
  static std::unique_ptr<ChangesetType> ParseChangesetFromString(
      const std::string& changeset);

  LazySyncable<ChangesetType>* inner_syncable_;

  DISALLOW_COPY_AND_ASSIGN(LazySyncableAdapter);
};

template <class ChangesetType>
std::unique_ptr<std::string>
LazySyncableAdapter<ChangesetType>::CreateChangeset(Revision from) const {
  std::unique_ptr<ChangesetType> changeset =
      inner_syncable_->CreateChangeset(from);
  std::unique_ptr<std::string> output = base::MakeUnique<std::string>();
  google::protobuf::io::StringOutputStream raw_output_stream(output.get());
  google::protobuf::io::CodedOutputStream output_stream(&raw_output_stream);
  changeset->Serialize(&output_stream);
  return output;
}

template <class ChangesetType>
void LazySyncableAdapter<ChangesetType>::ApplyChangeset(
    const std::string& changeset) {
  std::unique_ptr<ChangesetType> parsed_changeset =
      ParseChangesetFromString(changeset);
  inner_syncable_->ApplyChangeset(*parsed_changeset);
}

template <class ChangesetType>
bool LazySyncableAdapter<ChangesetType>::ValidateChangeset(
    const std::string& changeset) const {
  std::unique_ptr<ChangesetType> parsed_changeset =
      ParseChangesetFromString(changeset);
  return inner_syncable_->ValidateChangeset(*parsed_changeset);
}

template <class ChangesetType>
void LazySyncableAdapter<ChangesetType>::SetLocalUpdateCallback(
    const base::Closure& local_update_callback) {
  inner_syncable_->SetLocalUpdateCallback(local_update_callback);
}

template <class ChangesetType>
void LazySyncableAdapter<ChangesetType>::ReleaseBefore(Revision before) {
  inner_syncable_->ReleaseBefore(before);
}

template <class ChangesetType>
Revision LazySyncableAdapter<ChangesetType>::GetRevision() const {
  return inner_syncable_->GetRevision();
}

template <class ChangesetType>
void LazySyncableAdapter<ChangesetType>::PrepareToCreateChangeset(
    Revision from,
    base::Closure done) {
  inner_syncable_->PrepareToCreateChangeset(from, done);
}

template <class ChangesetType>
std::unique_ptr<ChangesetType>
LazySyncableAdapter<ChangesetType>::ParseChangesetFromString(
    const std::string& changeset) {
  google::protobuf::io::ArrayInputStream raw_input_stream(changeset.data(),
                                                          changeset.size());
  google::protobuf::io::CodedInputStream input_stream(&raw_input_stream);
  std::unique_ptr<ChangesetType> parsed_changeset =
      base::MakeUnique<ChangesetType>();
  parsed_changeset->Parse(&input_stream);
  return parsed_changeset;
}

}  // namespace helium
}  // namespace blimp
#endif  // BLIMP_HELIUM_LAZY_SYNCABLE_ADAPTER_H_
