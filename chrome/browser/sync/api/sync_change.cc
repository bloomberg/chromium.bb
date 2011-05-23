// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/api/sync_change.h"

SyncChange::SyncChange() : change_type_(ACTION_INVALID) {
}

SyncChange::SyncChange(SyncChangeType change_type, const SyncData& sync_data)
    : change_type_(change_type),
      sync_data_(sync_data) {
  DCHECK(IsValid());
}

SyncChange::~SyncChange() {}

bool SyncChange::IsValid() const {
  if (change_type_ == ACTION_INVALID || !sync_data_.IsValid())
    return false;

  // Data from the syncer must always have specifics.
  if (!sync_data_.IsLocal())
    return (sync_data_.GetDataType() != syncable::UNSPECIFIED);

  // Local changes must always have a tag.
  if (sync_data_.GetTag().empty())
    return false;

  // Adds and updates must have valid specifics (checked by GetDataType())
  if (change_type_ == ACTION_ADD || change_type_ == ACTION_UPDATE)
    return (sync_data_.GetDataType() != syncable::UNSPECIFIED);

  return true;
}

SyncChange::SyncChangeType SyncChange::change_type() const {
  return change_type_;
}

SyncData SyncChange::sync_data() const {
  return sync_data_;
}
