// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_HELIUM_COMPOUND_SYNCABLE_H_
#define BLIMP_HELIUM_COMPOUND_SYNCABLE_H_

#include <stdint.h>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/stl_util.h"
#include "blimp/helium/blimp_helium_export.h"
#include "blimp/helium/serializable_struct.h"
#include "blimp/helium/syncable.h"
#include "third_party/protobuf/src/google/protobuf/io/coded_stream.h"
#include "third_party/protobuf/src/google/protobuf/io/zero_copy_stream_impl_lite.h"

namespace blimp {
namespace helium {

struct CompoundChangeset;

// Base class for defining Syncables that are containers for other Syncables.
// It manages an internal registry of child Syncables that is declaratively
// populated at class construction time. It manages serialization and
// deserialization across its registered Syncables, combining the serialized
// output inside a simple map-based changeset.
//
// Example usage:
//
//   struct ParentSyncable : public CompoundSyncable {
//     explicit ParentSyncable()
//         : child1(CreateAndRegister<Child1>()),
//           child2(CreateAndRegister<Child2>()) {}
//
//     RegisteredSyncable<Child1> child1;
//     RegisteredSyncable<Child2> child2;
//
//    private:
//     DISALLOW_COPY_AND_ASSIGN(SampleCompoundSyncable);
//   };
//
class BLIMP_HELIUM_EXPORT CompoundSyncable
    : public Syncable<CompoundChangeset> {
 public:
  CompoundSyncable();
  ~CompoundSyncable() override;

  // Syncable implementation.
  std::unique_ptr<CompoundChangeset> CreateChangeset(
      Revision from) const override;
  void ApplyChangeset(const CompoundChangeset& changeset) override;
  void ReleaseBefore(Revision from) override;
  Revision GetRevision() const override;
  bool ValidateChangeset(const CompoundChangeset& changeset) const override;
  void SetLocalUpdateCallback(
      const base::Closure& local_update_callback) override;

 protected:
  // Defines the methods needed to access changesets and revision in Syncables
  // in a Syncable type-agnostic manner, so that they may be accessed in an
  // aggregated fashion using |members_|.
  class RegisteredSyncableBase {
   public:
    virtual ~RegisteredSyncableBase() = default;

    // Parses and validate a changeset from |input_stream|.
    // The parsed changeset is retained within |this| for later use by
    // ApplyChangeset().
    // Returns true if the parse and validation steps both succeded.
    virtual bool ParseAndValidate(
        google::protobuf::io::CodedInputStream* input_stream) = 0;
    virtual void ApplyChangeset() = 0;

    // Methods to be proxied directly to the Syncable implementation.
    virtual Revision GetRevision() = 0;
    virtual void CreateChangeset(
        Revision from,
        google::protobuf::io::CodedOutputStream* output_stream) const = 0;
    virtual void SetLocalUpdateCallback(
        const base::Closure& local_update_callback) = 0;
    virtual void ReleaseBefore(Revision revision) = 0;
  };

  template <typename SyncableType>
  class RegisteredSyncable : public RegisteredSyncableBase {
   public:
    ~RegisteredSyncable() = default;
    RegisteredSyncable(RegisteredSyncable<SyncableType>&& other) = default;

    SyncableType* get() { return member_.get(); }
    SyncableType* operator->() { return member_.get(); }

    // RegisteredSyncableBase implementation.
    bool ParseAndValidate(
        google::protobuf::io::CodedInputStream* input_stream) override;
    virtual void ApplyChangeset();
    Revision GetRevision() override;
    void CreateChangeset(
        Revision from,
        google::protobuf::io::CodedOutputStream* output_stream) const override;
    void SetLocalUpdateCallback(
        const base::Closure& local_update_callback) override;
    void ReleaseBefore(Revision revision) override;

   private:
    friend class CompoundSyncable;

    // Make class only constructable by CompoundSyncable, so that there is a
    // strong guarantee that a RegisteredSyncable was created by
    // SyncableRegistry::CreateAndRegister().
    explicit RegisteredSyncable(std::unique_ptr<SyncableType> member)
        : member_(std::move(member)) {}

    std::unique_ptr<SyncableType> member_;
    std::unique_ptr<typename SyncableType::Changeset> parsed_changeset_;

    DISALLOW_COPY_AND_ASSIGN(RegisteredSyncable);
  };

  template <typename T, typename... ConstructorArgs>
  RegisteredSyncable<T> CreateAndRegister(ConstructorArgs... args);

 private:
  // Tracks all Syncables* which have been created with CreateAndRegister().
  std::vector<RegisteredSyncableBase*> members_;

  DISALLOW_COPY_AND_ASSIGN(CompoundSyncable);
};

template <typename SyncableType>
bool CompoundSyncable::RegisteredSyncable<SyncableType>::ParseAndValidate(
    google::protobuf::io::CodedInputStream* input_stream) {
  DCHECK(!parsed_changeset_);
  parsed_changeset_ = base::MakeUnique<typename SyncableType::Changeset>();
  return (parsed_changeset_->Parse(input_stream) &&
          member_->ValidateChangeset(*parsed_changeset_));
}

template <typename SyncableType>
void CompoundSyncable::RegisteredSyncable<SyncableType>::ApplyChangeset() {
  DCHECK(parsed_changeset_);
  member_->ApplyChangeset(*parsed_changeset_);
  parsed_changeset_.reset();
}

template <typename SyncableType>
Revision CompoundSyncable::RegisteredSyncable<SyncableType>::GetRevision() {
  return member_->GetRevision();
}

template <typename SyncableType>
void CompoundSyncable::RegisteredSyncable<SyncableType>::CreateChangeset(
    Revision from,
    google::protobuf::io::CodedOutputStream* output_stream) const {
  std::unique_ptr<typename SyncableType::Changeset> changeset =
      member_->CreateChangeset(from);
  changeset->Serialize(output_stream);
}

template <typename SyncableType>
void CompoundSyncable::RegisteredSyncable<SyncableType>::SetLocalUpdateCallback(
    const base::Closure& local_update_callback) {
  member_->SetLocalUpdateCallback(local_update_callback);
}

template <typename SyncableType>
void CompoundSyncable::RegisteredSyncable<SyncableType>::ReleaseBefore(
    Revision revision) {
  member_->ReleaseBefore(revision);
}

template <typename SyncableType, typename... ConstructorArgs>
CompoundSyncable::RegisteredSyncable<SyncableType>
CompoundSyncable::CreateAndRegister(ConstructorArgs... args) {
  RegisteredSyncable<SyncableType> new_member(
      base::MakeUnique<SyncableType>(std::forward<ConstructorArgs>(args)...));
  members_.push_back(&new_member);
  return new_member;
}

// Stores the serialized changesets of all CompoundSyncable members.
struct BLIMP_HELIUM_EXPORT CompoundChangeset : public SerializableStruct {
 public:
  CompoundChangeset();
  ~CompoundChangeset() override;

  // Convenience function for parsing a serialized changeset as Changeset.
  template <typename ChangesetType>
  ChangesetType ParseForTest(int index) const;

  // Sparse representation of serialized changed fields, indexed by their
  // 0-based field index.
  Field<std::map<int32_t, std::string>> compound_changesets;

 private:
  DISALLOW_COPY_AND_ASSIGN(CompoundChangeset);
};

template <typename ChangesetType>
ChangesetType CompoundChangeset::ParseForTest(int index) const {
  DCHECK(base::ContainsKey(compound_changesets, index));

  google::protobuf::io::ArrayInputStream raw_input_stream(
      compound_changesets().at(index).data(),
      compound_changesets().at(index).size());
  google::protobuf::io::CodedInputStream input_stream(&raw_input_stream);

  ChangesetType output;
  CHECK(output.Parse(input_stream));
  return output;
}

}  // namespace helium
}  // namespace blimp

#endif  // BLIMP_HELIUM_COMPOUND_SYNCABLE_H_
