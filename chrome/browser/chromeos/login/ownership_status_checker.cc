// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/ownership_status_checker.h"

#include "base/bind.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace chromeos {

OwnershipStatusChecker::OwnershipStatusChecker() {
}

OwnershipStatusChecker::~OwnershipStatusChecker() {
  if (core_.get())
    core_->Cancel();
}

void OwnershipStatusChecker::Check(const Callback& callback) {
  // If already called once cancel the old callback.
  if (core_.get())
    core_->Cancel();
  core_ = new Core(callback);
  core_->Check();
}

OwnershipStatusChecker::Core::Core(const Callback& callback)
    : callback_(callback),
      origin_loop_(base::MessageLoopProxy::current()) {
}

OwnershipStatusChecker::Core::~Core() {}

void OwnershipStatusChecker::Core::Check() {
  DCHECK(origin_loop_->BelongsToCurrentThread());
  // We can only report the status after executing code on the file thread
  // because checking whether the current user is owner needs file access.
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&OwnershipStatusChecker::Core::CheckOnFileThread, this));
}

void OwnershipStatusChecker::Core::Cancel() {
  DCHECK(origin_loop_->BelongsToCurrentThread());
  callback_.Reset();
}

void OwnershipStatusChecker::Core::CheckOnFileThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  OwnershipService::Status status =
      OwnershipService::GetSharedInstance()->IsAlreadyOwned() ?
          OwnershipService::OWNERSHIP_TAKEN : OwnershipService::OWNERSHIP_NONE;
  bool current_user_is_owner =
      OwnershipService::GetSharedInstance()->CurrentUserIsOwner();
  origin_loop_->PostTask(
      FROM_HERE,
      base::Bind(&OwnershipStatusChecker::Core::ReportResult, this, status,
                 current_user_is_owner));
}

void OwnershipStatusChecker::Core::ReportResult(
    OwnershipService::Status status, bool current_user_is_owner) {
  DCHECK(origin_loop_->BelongsToCurrentThread());
  if (!callback_.is_null()) {
    callback_.Run(status, current_user_is_owner);
    callback_.Reset();
  }
}

}  // namespace chromeos
