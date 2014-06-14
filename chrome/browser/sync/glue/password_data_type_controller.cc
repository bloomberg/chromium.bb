// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/password_data_type_controller.h"

#include "base/bind.h"
#include "base/metrics/histogram.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/glue/chrome_report_unrecoverable_error.h"
#include "chrome/browser/sync/profile_sync_components_factory.h"
#include "components/password_manager/core/browser/password_store.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace browser_sync {

PasswordDataTypeController::PasswordDataTypeController(
    ProfileSyncComponentsFactory* profile_sync_factory,
    Profile* profile,
    const DisableTypeCallback& disable_callback)
    : NonUIDataTypeController(
          BrowserThread::GetMessageLoopProxyForThread(BrowserThread::UI),
          base::Bind(&ChromeReportUnrecoverableError),
          disable_callback,
          profile_sync_factory),
      profile_(profile) {
}

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
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!password_store_)
    return false;
  return password_store_->ScheduleTask(task);
}

bool PasswordDataTypeController::StartModels() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK_EQ(MODEL_STARTING, state());
  password_store_ = PasswordStoreFactory::GetForProfile(
      profile_, Profile::EXPLICIT_ACCESS);
  return !!password_store_;
}

}  // namespace browser_sync
