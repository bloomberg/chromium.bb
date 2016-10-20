// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_HELIUM_LWW_REGISTER_H_
#define BLIMP_HELIUM_LWW_REGISTER_H_

#include "base/memory/ptr_util.h"
#include "blimp/helium/blimp_helium_export.h"
#include "blimp/helium/coded_value_serializer.h"
#include "blimp/helium/result.h"
#include "blimp/helium/syncable.h"
#include "blimp/helium/syncable_common.h"
#include "blimp/helium/version_vector.h"
#include "blimp/helium/version_vector_generator.h"
#include "third_party/protobuf/src/google/protobuf/io/coded_stream.h"

namespace blimp {
namespace helium {

// Provides a simple syncable and atomically-writable "register" holding
// contents of type |RegisterType|. When there is a write conflict, it is
// resolved by assuming the writer indicated by |bias| has the correct value.
template <class RegisterType>
class BLIMP_HELIUM_EXPORT LwwRegister : public Syncable {
 public:
  LwwRegister(VersionVectorGenerator* version_gen, Peer bias, Peer running_as);
  ~LwwRegister() = default;

  void Set(const RegisterType& value);

  const RegisterType& Get() const;

  // Syncable implementation.
  void CreateChangesetToCurrent(
      Revision from,
      google::protobuf::io::CodedOutputStream* output_stream) override;
  Result ApplyChangeset(
      Revision to,
      google::protobuf::io::CodedInputStream* input_stream) override;
  void ReleaseBefore(Revision checkpoint) override;
  VersionVector GetVersionVector() const override;

 private:
  VersionVectorGenerator* version_gen_;
  VersionVector last_modified_;
  bool locally_owned_;
  RegisterType value_;
  bool value_set_ = false;

  DISALLOW_COPY_AND_ASSIGN(LwwRegister);
};

template <class RegisterType>
LwwRegister<RegisterType>::LwwRegister(VersionVectorGenerator* version_gen,
                                       Peer bias,
                                       Peer running_as)
    : version_gen_(version_gen),
      last_modified_(version_gen->current()),
      locally_owned_(bias == running_as) {
  DCHECK(version_gen_);
}

template <class RegisterType>
void LwwRegister<RegisterType>::Set(const RegisterType& value) {
  value_ = value;
  value_set_ = true;
  version_gen_->Increment();
  last_modified_ = last_modified_.MergeWith(version_gen_->current());
}

template <class RegisterType>
const RegisterType& LwwRegister<RegisterType>::Get() const {
  DCHECK(value_set_);
  return value_;
}

template <class RegisterType>
VersionVector LwwRegister<RegisterType>::GetVersionVector() const {
  return last_modified_;
}

template <class RegisterType>
void LwwRegister<RegisterType>::CreateChangesetToCurrent(
    Revision from,
    google::protobuf::io::CodedOutputStream* output_stream) {
  CodedValueSerializer::Serialize(last_modified_, output_stream);
  CodedValueSerializer::Serialize(value_, output_stream);
}

template <class RegisterType>
Result LwwRegister<RegisterType>::ApplyChangeset(
    Revision to,
    google::protobuf::io::CodedInputStream* input_stream) {
  VersionVector remote;
  if (!CodedValueSerializer::Deserialize(input_stream, &remote)) {
    return Result::ERR_INTERNAL_ERROR;
  }
  remote = remote.Invert();
  VersionVector::Comparison cmp = last_modified_.CompareTo(remote);
  if (cmp == VersionVector::Comparison::LessThan ||
      (cmp == VersionVector::Comparison::Conflict && !locally_owned_)) {
    if (!CodedValueSerializer::Deserialize(input_stream, &value_)) {
      return Result::ERR_INTERNAL_ERROR;
    }
    value_set_ = true;
  }
  last_modified_ = last_modified_.MergeWith(remote);
  return Result::SUCCESS;
}

template <class RegisterType>
void LwwRegister<RegisterType>::ReleaseBefore(Revision checkpoint) {
  // no-op
}

}  // namespace helium
}  // namespace blimp

#endif  // BLIMP_HELIUM_LWW_REGISTER_H_
