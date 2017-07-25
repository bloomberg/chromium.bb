// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_CORE_BROWSER_SCOPED_ACCOUNT_CONSISTENCY_H_
#define COMPONENTS_SIGNIN_CORE_BROWSER_SCOPED_ACCOUNT_CONSISTENCY_H_

#include <memory>

#include "base/macros.h"
#include "base/test/scoped_feature_list.h"
#include "components/signin/core/common/profile_management_switches.h"

namespace signin {

// Changes the account consistency method while it is in scope. Useful for
// tests.
class ScopedAccountConsistency {
 public:
  ScopedAccountConsistency(AccountConsistencyMethod method);

  ~ScopedAccountConsistency();

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(ScopedAccountConsistency);
};

class ScopedAccountConsistencyMirror {
 public:
  ScopedAccountConsistencyMirror();

 private:
  ScopedAccountConsistency scoped_mirror_;

  DISALLOW_COPY_AND_ASSIGN(ScopedAccountConsistencyMirror);
};

class ScopedAccountConsistencyDice {
 public:
  ScopedAccountConsistencyDice();

 private:
  ScopedAccountConsistency scoped_dice_;

  DISALLOW_COPY_AND_ASSIGN(ScopedAccountConsistencyDice);
};

}  // namespace signin

#endif  // COMPONENTS_SIGNIN_CORE_BROWSER_SCOPED_ACCOUNT_CONSISTENCY_H_
