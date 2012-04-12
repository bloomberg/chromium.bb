// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/tpm_password_fetcher.h"

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/message_loop.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/cryptohome_library.h"

namespace chromeos {

namespace {

// Interval between TPM password checks.
const int kTpmCheckIntervalMs = 500;

}  // namespace

TpmPasswordFetcher::TpmPasswordFetcher(TpmPasswordFetcherDelegate* delegate)
    : ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)),
      delegate_(delegate) {
  DCHECK(delegate_);
}

TpmPasswordFetcher::~TpmPasswordFetcher() {
}

void TpmPasswordFetcher::Fetch() {
  // Since this method is also called directly.
  weak_factory_.InvalidateWeakPtrs();

  std::string password;

  chromeos::CryptohomeLibrary* cryptohome =
      chromeos::CrosLibrary::Get()->GetCryptohomeLibrary();

  if (cryptohome->TpmIsReady() && cryptohome->TpmGetPassword(&password)) {
    if (password.empty()) {
      // For a fresh OOBE flow TPM is uninitialized,
      // ownership process is started at the EULA screen,
      // password is cleared after EULA is accepted.
      LOG(ERROR) << "TPM returned an empty password.";
    }
    delegate_->OnPasswordFetched(password);
  } else {
    // Password hasn't been acquired, reschedule fetch.
    MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&TpmPasswordFetcher::Fetch, weak_factory_.GetWeakPtr()),
        base::TimeDelta::FromMilliseconds(kTpmCheckIntervalMs));
  }
}

}  // namespace chromeos
