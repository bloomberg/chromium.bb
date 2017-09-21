// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/tpm/tpm_password_fetcher.h"

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chromeos/dbus/cryptohome_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"

namespace chromeos {

namespace {

// Interval between TPM password checks.
const int kTpmCheckIntervalMs = 500;

}  // namespace

TpmPasswordFetcher::TpmPasswordFetcher(TpmPasswordFetcherDelegate* delegate)
    : delegate_(delegate), weak_factory_(this) {
  DCHECK(delegate_);
}

TpmPasswordFetcher::~TpmPasswordFetcher() {
}

void TpmPasswordFetcher::Fetch() {
  // Since this method is also called directly.
  weak_factory_.InvalidateWeakPtrs();

  DBusThreadManager::Get()->GetCryptohomeClient()->TpmIsReady(base::BindOnce(
      &TpmPasswordFetcher::OnTpmIsReady, weak_factory_.GetWeakPtr()));
}

void TpmPasswordFetcher::OnTpmIsReady(base::Optional<bool> tpm_is_ready) {
  if (tpm_is_ready.value_or(false)) {
    DBusThreadManager::Get()->GetCryptohomeClient()->TpmGetPassword(
        base::BindOnce(&TpmPasswordFetcher::OnTpmGetPassword,
                       weak_factory_.GetWeakPtr()));
  } else {
    // Password hasn't been acquired, reschedule fetch.
    RescheduleFetch();
  }
}

void TpmPasswordFetcher::OnTpmGetPassword(
    base::Optional<std::string> password) {
  if (password) {
    if (password->empty()) {
      // For a fresh OOBE flow TPM is uninitialized,
      // ownership process is started at the EULA screen,
      // password is cleared after EULA is accepted.
      LOG(ERROR) << "TPM returned an empty password.";
    }
    delegate_->OnPasswordFetched(*password);
  } else {
    // Password hasn't been acquired, reschedule fetch.
    RescheduleFetch();
  }
}

void TpmPasswordFetcher::RescheduleFetch() {
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&TpmPasswordFetcher::Fetch, weak_factory_.GetWeakPtr()),
      base::TimeDelta::FromMilliseconds(kTpmCheckIntervalMs));
}

}  // namespace chromeos
