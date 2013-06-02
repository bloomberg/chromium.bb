// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/startup_task_runner_service.h"

#include "base/deferred_sequenced_task_runner.h"
#include "base/logging.h"
#include "chrome/browser/profiles/profile.h"

StartupTaskRunnerService::StartupTaskRunnerService(Profile* profile)
    : profile_(profile) {
}

StartupTaskRunnerService::~StartupTaskRunnerService() {
}

scoped_refptr<base::DeferredSequencedTaskRunner>
    StartupTaskRunnerService::GetBookmarkTaskRunner() {
  DCHECK(CalledOnValidThread());
  if (!bookmark_task_runner_.get()) {
    bookmark_task_runner_ =
        new base::DeferredSequencedTaskRunner(profile_->GetIOTaskRunner());
  }
  return bookmark_task_runner_;
}

void StartupTaskRunnerService::StartDeferredTaskRunners() {
  GetBookmarkTaskRunner()->Start();
}
