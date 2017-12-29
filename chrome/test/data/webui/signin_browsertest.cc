// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/data/webui/signin_browsertest.h"

#include "components/signin/core/browser/profile_management_switches.h"
#include "components/signin/core/browser/scoped_account_consistency.h"

SigninBrowserTest::SigninBrowserTest() {}

SigninBrowserTest::~SigninBrowserTest() {}

void SigninBrowserTest::EnableDice() {
  scoped_account_consistency_ =
      base::MakeUnique<signin::ScopedAccountConsistency>(
          signin::AccountConsistencyMethod::kDice);
}
