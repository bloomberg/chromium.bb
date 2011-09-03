// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/ownership_status_checker.h"

#include "content/browser/browser_thread.h"

namespace chromeos {

OwnershipStatusChecker::OwnershipStatusChecker(Callback* callback)
    : core_(new Core(callback)) {
  core_->Check();
}

OwnershipStatusChecker::~OwnershipStatusChecker() {
  core_->Cancel();
}

OwnershipStatusChecker::Core::Core(Callback* callback)
    : callback_(callback),
      origin_loop_(base::MessageLoopProxy::current()) {
}

OwnershipStatusChecker::Core::~Core() {}

void OwnershipStatusChecker::Core::Check() {
  DCHECK(origin_loop_->BelongsToCurrentThread());
  OwnershipService::Status status =
      OwnershipService::GetSharedInstance()->GetStatus(false);
  // We can only report the OWNERSHIP_NONE status without executing code on the
  // file thread because checking whether the current user is owner needs file
  // access.
  if (status == OwnershipService::OWNERSHIP_NONE) {
    // Take a spin on the message loop in order to avoid reentrancy in callers.
    origin_loop_->PostTask(
        FROM_HERE,
        NewRunnableMethod(this,
                          &OwnershipStatusChecker::Core::ReportResult,
                          status, false));
  } else {
    // Switch to the file thread to make the blocking call.
    BrowserThread::PostTask(
        BrowserThread::FILE, FROM_HERE,
        NewRunnableMethod(this,
                          &OwnershipStatusChecker::Core::CheckOnFileThread));
  }
}

void OwnershipStatusChecker::Core::Cancel() {
  DCHECK(origin_loop_->BelongsToCurrentThread());
  callback_.reset();
}

void OwnershipStatusChecker::Core::CheckOnFileThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  OwnershipService::Status status =
      OwnershipService::GetSharedInstance()->GetStatus(true);
  bool current_user_is_owner =
      OwnershipService::GetSharedInstance()->CurrentUserIsOwner();
  origin_loop_->PostTask(
      FROM_HERE,
      NewRunnableMethod(this,
                        &OwnershipStatusChecker::Core::ReportResult,
                        status, current_user_is_owner));
}

void OwnershipStatusChecker::Core::ReportResult(
    OwnershipService::Status status, bool current_user_is_owner) {
  DCHECK(origin_loop_->BelongsToCurrentThread());
  if (callback_.get()) {
    callback_->Run(status, current_user_is_owner);
    callback_.reset();
  }
}

}  // namespace chromeos
