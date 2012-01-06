// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_TPM_PASSWORD_FETCHER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_TPM_PASSWORD_FETCHER_H_

#include <string>

#include "base/basictypes.h"
#include "base/memory/weak_ptr.h"

namespace chromeos {

// Interface which TpmPasswordFetcher uses to notify that password has been
// fetched.
class TpmPasswordFetcherDelegate {
 public:
  virtual ~TpmPasswordFetcherDelegate() {}
  virtual void OnPasswordFetched(const std::string& tpm_password) = 0;
};

// Class for fetching TPM password from the Cryptohome.
class TpmPasswordFetcher {
 public:
  // Creates fetcher with the given delegate to be notified every time fetching
  // is done.
  explicit TpmPasswordFetcher(TpmPasswordFetcherDelegate* delegate);
  ~TpmPasswordFetcher();

  // Fetches TPM password and stores the result. Also notifies |delegate_| with
  // OnPasswordFetched() call.
  void Fetch();

 private:
  base::WeakPtrFactory<TpmPasswordFetcher> weak_factory_;
  TpmPasswordFetcherDelegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(TpmPasswordFetcher);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_TPM_PASSWORD_FETCHER_H_
