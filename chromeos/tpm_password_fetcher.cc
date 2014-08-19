// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tpm_password_fetcher.h"

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/message_loop/message_loop.h"
#include "chromeos/dbus/cryptohome_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"

namespace chromeos {

namespace {

// Interval between TPM password checks.
const int kTpmCheckIntervalMs = 500;

}  // namespace

TpmPasswordFetcher::TpmPasswordFetcher(TpmPasswordFetcherDelegate* delegate)
    : weak_factory_(this), delegate_(delegate) {
  DCHECK(delegate_);
}

TpmPasswordFetcher::~TpmPasswordFetcher() {
}

void TpmPasswordFetcher::Fetch() {
  // Since this method is also called directly.
  weak_factory_.InvalidateWeakPtrs();

  DBusThreadManager::Get()->GetCryptohomeClient()->TpmIsReady(base::Bind(
      &TpmPasswordFetcher::OnTpmIsReady, weak_factory_.GetWeakPtr()));
}

void TpmPasswordFetcher::OnTpmIsReady(DBusMethodCallStatus call_status,
                                      bool tpm_is_ready) {
  if (call_status == DBUS_METHOD_CALL_SUCCESS && tpm_is_ready) {
    DBusThreadManager::Get()->GetCryptohomeClient()->TpmGetPassword(base::Bind(
        &TpmPasswordFetcher::OnTpmGetPassword, weak_factory_.GetWeakPtr()));
  } else {
    // Password hasn't been acquired, reschedule fetch.
    RescheduleFetch();
  }
}

void TpmPasswordFetcher::OnTpmGetPassword(DBusMethodCallStatus call_status,
                                          const std::string& password) {
  if (call_status == DBUS_METHOD_CALL_SUCCESS) {
    if (password.empty()) {
      // For a fresh OOBE flow TPM is uninitialized,
      // ownership process is started at the EULA screen,
      // password is cleared after EULA is accepted.
      LOG(ERROR) << "TPM returned an empty password.";
    }
    delegate_->OnPasswordFetched(password);
  } else {
    // Password hasn't been acquired, reschedule fetch.
    RescheduleFetch();
  }
}

void TpmPasswordFetcher::RescheduleFetch() {
  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&TpmPasswordFetcher::Fetch, weak_factory_.GetWeakPtr()),
      base::TimeDelta::FromMilliseconds(kTpmCheckIntervalMs));
}

}  // namespace chromeos
