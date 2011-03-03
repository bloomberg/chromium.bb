// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/time.h"
#include "chrome/browser/sync/engine/syncer_thread_adapter.h"
#include "chrome/browser/sync/syncable/model_type.h"

using base::TimeDelta;
using syncable::ModelTypeBitSet;

namespace browser_sync {

SyncerThreadAdapter::SyncerThreadAdapter(sessions::SyncSessionContext* context,
                                         bool using_new_impl)
    : legacy_(NULL), new_impl_(NULL), using_new_impl_(using_new_impl) {
  if (using_new_impl_) {
    new_impl_.reset(new s3::SyncerThread(context, new Syncer()));
    new_impl_->Start(s3::SyncerThread::CONFIGURATION_MODE);
  } else {
    legacy_ = new SyncerThread(context);
  }
}

SyncerThreadAdapter::~SyncerThreadAdapter() {
  legacy_ = NULL;
  new_impl_.reset();
}

void SyncerThreadAdapter::WatchConnectionManager(
    ServerConnectionManager* conn_mgr) {
  DCHECK(!using_new_impl_);
  legacy_->WatchConnectionManager(conn_mgr);
}

bool SyncerThreadAdapter::Start() {
  if (using_new_impl_) {
    new_impl_->Start(s3::SyncerThread::NORMAL_MODE);
    return true;
  } else {
    return legacy_->Start();
  }
}

bool SyncerThreadAdapter::Stop(int max_wait) {
  if (using_new_impl_) {
    new_impl_->Stop();
    return true;
  } else {
    return legacy_->Stop(max_wait);
  }
}

bool SyncerThreadAdapter::RequestPause() {
  DCHECK(!using_new_impl_);
  return legacy_->RequestPause();
}

bool SyncerThreadAdapter::RequestResume() {
  DCHECK(!using_new_impl_);
  return legacy_->RequestResume();
}

s3::NudgeSource LegacyToNewSyncerThreadSource(SyncerThread::NudgeSource s) {
  switch(s) {
    case SyncerThread::kNotification:
      return s3::NUDGE_SOURCE_NOTIFICATION;
    case SyncerThread::kContinuation:
      return s3::NUDGE_SOURCE_CONTINUATION;
    case SyncerThread::kLocal:
      return s3::NUDGE_SOURCE_LOCAL;
    case SyncerThread::kUnknown:
      return s3::NUDGE_SOURCE_UNKNOWN;
    default:
      NOTREACHED();
      return s3::NUDGE_SOURCE_UNKNOWN;
  }
}

void SyncerThreadAdapter::NudgeSyncer(int milliseconds_from_now,
                                      SyncerThread::NudgeSource source) {
  if (using_new_impl_) {
    if (source == SyncerThread::kClearPrivateData) {
      new_impl_->ScheduleClearUserData();
      return;
    }
    new_impl_->ScheduleNudge(
        TimeDelta::FromMilliseconds(milliseconds_from_now),
        LegacyToNewSyncerThreadSource(source), ModelTypeBitSet());
  } else {
    legacy_->NudgeSyncer(milliseconds_from_now, source);
  }
}

void SyncerThreadAdapter::NudgeSyncerWithDataTypes(
      int milliseconds_from_now,
      SyncerThread::NudgeSource source,
      const syncable::ModelTypeBitSet& model_types) {
  DCHECK_NE(SyncerThread::kClearPrivateData, source);
  if (using_new_impl_) {
    new_impl_->ScheduleNudge(
        TimeDelta::FromMilliseconds(milliseconds_from_now),
        LegacyToNewSyncerThreadSource(source), model_types);
  } else {
    legacy_->NudgeSyncerWithDataTypes(milliseconds_from_now, source,
        model_types);
  }
}

void SyncerThreadAdapter::NudgeSyncerWithPayloads(
      int milliseconds_from_now,
      SyncerThread::NudgeSource source,
      const sessions::TypePayloadMap& model_types_with_payloads) {
  DCHECK_NE(SyncerThread::kClearPrivateData, source);
  if (using_new_impl_) {
    new_impl_->ScheduleNudgeWithPayloads(
        TimeDelta::FromMilliseconds(milliseconds_from_now),
        LegacyToNewSyncerThreadSource(source), model_types_with_payloads);
  } else {
    legacy_->NudgeSyncerWithPayloads(milliseconds_from_now, source,
        model_types_with_payloads);
  }
}

void SyncerThreadAdapter::SetNotificationsEnabled(bool enabled) {
  if (using_new_impl_)
    new_impl_->set_notifications_enabled(enabled);
  else
    legacy_->SetNotificationsEnabled(enabled);
}

void SyncerThreadAdapter::CreateSyncer(const std::string& dirname) {
  if (!using_new_impl_)
    legacy_->CreateSyncer(dirname);
  // No-op if using new impl.
}

s3::SyncerThread* SyncerThreadAdapter::new_impl() {
  return new_impl_.get();
}

}  // namespace browser_sync
