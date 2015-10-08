// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/supervised_user_sync_data_type_controller.h"

#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_thread.h"

SupervisedUserSyncDataTypeController::SupervisedUserSyncDataTypeController(
    syncer::ModelType type,
    const base::Closure& error_callback,
    sync_driver::SyncClient* sync_client,
    Profile* profile)
    : sync_driver::UIDataTypeController(
          content::BrowserThread::GetMessageLoopProxyForThread(
              content::BrowserThread::UI),
          error_callback,
          type,
          sync_client),
      profile_(profile) {
  DCHECK(type == syncer::SUPERVISED_USERS ||
         type == syncer::SUPERVISED_USER_SETTINGS ||
         type == syncer::SUPERVISED_USER_SHARED_SETTINGS ||
         type == syncer::SUPERVISED_USER_WHITELISTS);
}

SupervisedUserSyncDataTypeController::~SupervisedUserSyncDataTypeController() {}

bool SupervisedUserSyncDataTypeController::ReadyForStart() const {
  switch (type()) {
    case syncer::SUPERVISED_USERS:
      return !profile_->IsSupervised();
    case syncer::SUPERVISED_USER_SETTINGS:
    case syncer::SUPERVISED_USER_WHITELISTS:
      return profile_->IsSupervised();
    case syncer::SUPERVISED_USER_SHARED_SETTINGS:
      return !profile_->IsChild();
    default:
      NOTREACHED();
  }
  return false;
}
