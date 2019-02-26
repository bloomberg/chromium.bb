// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/fake_signin_manager.h"

#include "base/callback_helpers.h"
#include "build/build_config.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "components/signin/core/browser/signin_client.h"
#include "components/signin/core/browser/signin_metrics.h"
#include "components/signin/core/browser/signin_pref_names.h"

#if !defined(OS_CHROMEOS)

FakeSigninManager::FakeSigninManager(
    SigninClient* client,
    ProfileOAuth2TokenService* token_service,
    AccountTrackerService* account_tracker_service,
    GaiaCookieManagerService* cookie_manager_service)
    : FakeSigninManager(client,
                        token_service,
                        account_tracker_service,
                        cookie_manager_service,
                        signin::AccountConsistencyMethod::kDisabled) {}

FakeSigninManager::FakeSigninManager(
    SigninClient* client,
    ProfileOAuth2TokenService* token_service,
    AccountTrackerService* account_tracker_service,
    GaiaCookieManagerService* cookie_manager_service,
    signin::AccountConsistencyMethod account_consistency)
    : SigninManager(client,
                    token_service,
                    account_tracker_service,
                    cookie_manager_service,
                    account_consistency) {}

FakeSigninManager::~FakeSigninManager() {}

#endif  // !defined (OS_CHROMEOS)
