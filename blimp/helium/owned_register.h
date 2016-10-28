// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_HELIUM_OWNED_REGISTER_H_
#define BLIMP_HELIUM_OWNED_REGISTER_H_

#include "base/optional.h"
#include "blimp/helium/blimp_helium_export.h"
#include "blimp/helium/coded_value_serializer.h"
#include "blimp/helium/result.h"
#include "blimp/helium/revision_generator.h"
#include "blimp/helium/syncable.h"
#include "blimp/helium/syncable_common.h"
#include "blimp/helium/version_vector.h"
#include "third_party/protobuf/src/google/protobuf/io/coded_stream.h"

namespace blimp {
namespace helium {

// A Syncable that gives write permissions only to the "owner" Peer,
// while the other Peer only receives and applies updates.
template <class RegisterType>
class OwnedRegister : public Syncable {
 public:
  OwnedRegister(Peer running_as, Peer owner);
  ~OwnedRegister() = default;

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
  Revision last_modified_ = 0;
  bool locally_owned_;
  base::Optional<RegisterType> value_;

  DISALLOW_COPY_AND_ASSIGN(OwnedRegister);
};

template <class RegisterType>
OwnedRegister<RegisterType>::OwnedRegister(Peer running_as, Peer owner)
    : locally_owned_(owner == running_as) {}

template <class RegisterType>
void OwnedRegister<RegisterType>::Set(const RegisterType& value) {
  DCHECK(locally_owned_);

  value_ = value;
  last_modified_ = GetNextRevision();
}

template <class RegisterType>
const RegisterType& OwnedRegister<RegisterType>::Get() const {
  DCHECK(value_);
  return *value_;
}

template <class RegisterType>
VersionVector OwnedRegister<RegisterType>::GetVersionVector() const {
  VersionVector version = VersionVector(last_modified_, 0);
  if (locally_owned_) {
    return version;
  } else {
    return version.Invert();
  }
}

template <class RegisterType>
void OwnedRegister<RegisterType>::CreateChangesetToCurrent(
    Revision from,
    google::protobuf::io::CodedOutputStream* output_stream) {
  DCHECK(locally_owned_);
  DCHECK(output_stream);

  CodedValueSerializer::Serialize(last_modified_, output_stream);
  CodedValueSerializer::Serialize(*value_, output_stream);
}

template <class RegisterType>
Result OwnedRegister<RegisterType>::ApplyChangeset(
    Revision to,
    google::protobuf::io::CodedInputStream* input_stream) {
  DCHECK(input_stream);

  if (locally_owned_) {
    return Result::ERR_PROTOCOL_ERROR;
  }

  Revision remote;
  if (!CodedValueSerializer::Deserialize(input_stream, &remote)) {
    return Result::ERR_PROTOCOL_ERROR;
  }

  RegisterType input_value;
  if (!CodedValueSerializer::Deserialize(input_stream, &input_value)) {
    return Result::ERR_PROTOCOL_ERROR;
  }
  if (last_modified_ > remote) {
    return Result::ERR_PROTOCOL_ERROR;
  }
  if (last_modified_ < remote) {
    value_ = input_value;
    last_modified_ = remote;
  }
  return Result::SUCCESS;
}

template <class RegisterType>
void OwnedRegister<RegisterType>::ReleaseBefore(Revision checkpoint) {
  // no-op
}

}  // namespace helium
}  // namespace blimp

#endif  // BLIMP_HELIUM_OWNED_REGISTER_H_
