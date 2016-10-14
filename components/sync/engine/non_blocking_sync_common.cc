// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine/non_blocking_sync_common.h"

namespace syncer {

CommitRequestData::CommitRequestData() {}

CommitRequestData::CommitRequestData(const CommitRequestData& other) = default;

CommitRequestData::~CommitRequestData() {}

CommitResponseData::CommitResponseData() {}

CommitResponseData::CommitResponseData(const CommitResponseData& other) =
    default;

CommitResponseData::~CommitResponseData() {}

UpdateResponseData::UpdateResponseData() {}

UpdateResponseData::UpdateResponseData(const UpdateResponseData& other) =
    default;

UpdateResponseData::~UpdateResponseData() {}

}  // namespace syncer
