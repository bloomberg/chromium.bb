// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_HELIUM_LWW_REGISTER_H_
#define BLIMP_HELIUM_LWW_REGISTER_H_

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
struct LwwRegisterChangeset;

// Defines a register that obeys last-writer-wins semantics.
// In the event of a write conflict, the state of the locally-biased LwwRegister
// "wins".
template <class RegisterType>
class LwwRegister : public Syncable<LwwRegisterChangeset<RegisterType>> {
 public:
  using Changeset =
      typename Syncable<LwwRegisterChangeset<RegisterType>>::Changeset;

  LwwRegister(Peer owner, Peer running_as);
  ~LwwRegister() = default;

  void Set(const RegisterType& value);
  const RegisterType& Get() const;

  // Syncable implementation.
  std::unique_ptr<Changeset> CreateChangeset(Revision from) const override;
  void ApplyChangeset(const Changeset& changeset) override;
  void ReleaseBefore(Revision checkpoint) override;
  Revision GetRevision() const override;
  void SetLocalUpdateCallback(
      const base::Closure& local_update_callback) override;

 private:
  base::Closure local_update_callback_;
  VersionVector last_modified_;
  Bias bias_;
  RegisterType value_;

  DISALLOW_COPY_AND_ASSIGN(LwwRegister);
};

template <class RegisterType>
LwwRegister<RegisterType>::LwwRegister(Peer owner, Peer running_as)
    : bias_(ComputeBias(owner, running_as)) {}

template <class RegisterType>
void LwwRegister<RegisterType>::Set(const RegisterType& value) {
  value_ = value;
  last_modified_.set_local_revision(GetNextRevision());
  local_update_callback_.Run();
}

template <class RegisterType>
const RegisterType& LwwRegister<RegisterType>::Get() const {
  return value_;
}

template <class RegisterType>
Revision LwwRegister<RegisterType>::GetRevision() const {
  return last_modified_.local_revision();
}

template <class RegisterType>
void LwwRegister<RegisterType>::SetLocalUpdateCallback(
    const base::Closure& local_update_callback) {
  local_update_callback_ = local_update_callback;
}

template <class RegisterType>
std::unique_ptr<typename LwwRegister<RegisterType>::Changeset>
LwwRegister<RegisterType>::CreateChangeset(Revision from) const {
  std::unique_ptr<Changeset> changeset = base::MakeUnique<Changeset>();
  changeset->last_modified.Set(last_modified_);
  changeset->value.Set(value_);
  return changeset;
}

template <class RegisterType>
void LwwRegister<RegisterType>::ApplyChangeset(
    const LwwRegister<RegisterType>::Changeset& changeset) {
  const VersionVector remote_last_modified = changeset.last_modified().Invert();

  if (last_modified_.CompareWithBias(remote_last_modified, bias_) ==
      VersionVector::Comparison::LessThan) {
    value_ = changeset.value();
  }
  last_modified_ = last_modified_.MergeWith(remote_last_modified);
}

template <class RegisterType>
void LwwRegister<RegisterType>::ReleaseBefore(Revision checkpoint) {
  // no-op
}

template <class RegisterType>
struct LwwRegisterChangeset : public SerializableStruct {
  LwwRegisterChangeset() : last_modified(this), value(this) {}
  ~LwwRegisterChangeset() override {}

  Field<VersionVector> last_modified;
  Field<RegisterType> value;
};

}  // namespace helium
}  // namespace blimp

#endif  // BLIMP_HELIUM_LWW_REGISTER_H_
