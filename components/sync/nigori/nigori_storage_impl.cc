// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/nigori/nigori_storage_impl.h"

#include <string>

namespace syncer {

NigoriStorageImpl::NigoriStorageImpl(const base::FilePath& path,
                                     const Encryptor* encryptor) {
  NOTIMPLEMENTED();
}

NigoriStorageImpl::~NigoriStorageImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void NigoriStorageImpl::StoreData(const sync_pb::NigoriLocalData& data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTIMPLEMENTED();
}

base::Optional<sync_pb::NigoriLocalData> NigoriStorageImpl::RestoreData() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTIMPLEMENTED();
  return base::nullopt;
}

}  // namespace syncer
