// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/password_data_type_controller.h"

#include "base/bind.h"
#include "base/metrics/histogram.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/sync_driver/sync_client.h"
#include "components/sync_driver/sync_service.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace browser_sync {

PasswordDataTypeController::PasswordDataTypeController(
    const base::Closure& error_callback,
    sync_driver::SyncClient* sync_client,
    Profile* profile)
    : NonUIDataTypeController(
          BrowserThread::GetMessageLoopProxyForThread(BrowserThread::UI),
          error_callback,
          sync_client),
      sync_client_(sync_client),
      profile_(profile) {}

syncer::ModelType PasswordDataTypeController::type() const {
  return syncer::PASSWORDS;
}

syncer::ModelSafeGroup PasswordDataTypeController::model_safe_group()
    const {
  return syncer::GROUP_PASSWORD;
}

PasswordDataTypeController::~PasswordDataTypeController() {}

bool PasswordDataTypeController::PostTaskOnBackendThread(
      const tracked_objects::Location& from_here,
      const base::Closure& task) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!password_store_.get())
    return false;
  return password_store_->ScheduleTask(task);
}

bool PasswordDataTypeController::StartModels() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_EQ(MODEL_STARTING, state());

  sync_client_->GetSyncService()->AddObserver(this);

  OnStateChanged();

  password_store_ = sync_client_->GetPasswordStore();
  return !!password_store_.get();
}

void PasswordDataTypeController::StopModels() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  sync_client_->GetSyncService()->RemoveObserver(this);
}

void PasswordDataTypeController::OnStateChanged() {
  PasswordStoreFactory::OnPasswordsSyncedStatePotentiallyChanged(profile_);
}

}  // namespace browser_sync
