// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_HELIUM_OWNED_REGISTER_H_
#define BLIMP_HELIUM_OWNED_REGISTER_H_

#include <memory>

#include "base/memory/ptr_util.h"
#include "blimp/helium/blimp_helium_export.h"
#include "blimp/helium/result.h"
#include "blimp/helium/revision_generator.h"
#include "blimp/helium/serializable_struct.h"
#include "blimp/helium/stream_helpers.h"
#include "blimp/helium/syncable.h"
#include "blimp/helium/syncable_common.h"
#include "blimp/helium/version_vector.h"
#include "third_party/protobuf/src/google/protobuf/io/coded_stream.h"

namespace blimp {
namespace helium {

template <class RegisterType>
struct OwnedRegisterChangeset : public SerializableStruct {
  OwnedRegisterChangeset() : last_modified(this), value(this) {}
  ~OwnedRegisterChangeset() override = default;

  Field<Revision> last_modified;
  Field<RegisterType> value;
};

// A Syncable that gives write permissions only to the "owner" Peer,
// while the other Peer only receives and applies updates.
template <class RegisterType>
class OwnedRegister : public Syncable<OwnedRegisterChangeset<RegisterType>> {
 public:
  using ChangesetType = OwnedRegisterChangeset<RegisterType>;

  OwnedRegister(Peer running_as, Peer owner);
  ~OwnedRegister() = default;

  void Set(const RegisterType& value);
  const RegisterType& Get() const;

  // Syncable implementation.
  std::unique_ptr<ChangesetType> CreateChangeset(Revision from) const override;
  void ApplyChangeset(const ChangesetType& changeset) override;
  void ReleaseBefore(Revision checkpoint) override;
  bool ValidateChangeset(const ChangesetType& changeset) const override;
  Revision GetRevision() const override;
  void SetLocalUpdateCallback(
      const base::Closure& local_update_callback) override;

 private:
  base::Closure local_update_callback_;
  Revision last_modified_ = 0;
  Bias bias_;
  RegisterType value_ = {};

  DISALLOW_COPY_AND_ASSIGN(OwnedRegister);
};

template <class RegisterType>
OwnedRegister<RegisterType>::OwnedRegister(Peer owner, Peer running_as)
    : bias_(ComputeBias(owner, running_as)) {}

template <class RegisterType>
void OwnedRegister<RegisterType>::SetLocalUpdateCallback(
    const base::Closure& local_update_callback) {
  local_update_callback_ = local_update_callback;
}

template <class RegisterType>
void OwnedRegister<RegisterType>::Set(const RegisterType& value) {
  DCHECK(bias_ == Bias::LOCAL);

  value_ = value;
  last_modified_ = GetNextRevision();

  local_update_callback_.Run();
}

template <class RegisterType>
const RegisterType& OwnedRegister<RegisterType>::Get() const {
  return value_;
}

template <class RegisterType>
Revision OwnedRegister<RegisterType>::GetRevision() const {
  if (bias_ == Bias::LOCAL) {
    return last_modified_;
  } else {
    return 0;
  }
}

template <class RegisterType>
std::unique_ptr<typename OwnedRegister<RegisterType>::ChangesetType>
OwnedRegister<RegisterType>::CreateChangeset(Revision from) const {
  DCHECK(bias_ == Bias::LOCAL);

  std::unique_ptr<ChangesetType> changeset = base::MakeUnique<ChangesetType>();
  changeset->last_modified.Set(last_modified_);
  changeset->value.Set(value_);
  return changeset;
}

template <class RegisterType>
void OwnedRegister<RegisterType>::ApplyChangeset(
    const ChangesetType& changeset) {
  if (last_modified_ < changeset.last_modified()) {
    value_ = changeset.value();
    last_modified_ = changeset.last_modified();
  }
}

template <class RegisterType>
void OwnedRegister<RegisterType>::ReleaseBefore(Revision checkpoint) {
  // no-op
}

template <class RegisterType>
bool OwnedRegister<RegisterType>::ValidateChangeset(
    const ChangesetType& changeset) const {
  if (bias_ == Bias::LOCAL) {
    return false;
  }

  if (last_modified_ > changeset.last_modified()) {
    return false;
  }

  return true;
}

}  // namespace helium
}  // namespace blimp

#endif  // BLIMP_HELIUM_OWNED_REGISTER_H_
