// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_HTTP_DATA_CLEANER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_HTTP_DATA_CLEANER_H_

#include "base/memory/ref_counted.h"
#include "net/url_request/url_request_context_getter.h"

class PrefService;

namespace password_manager {

class PasswordStore;

void DelayCleanObsoleteHttpDataForPasswordStoreAndPrefs(
    PasswordStore* store,
    PrefService* prefs,
    const scoped_refptr<net::URLRequestContextGetter>& request_context);

void CleanObsoleteHttpDataForPasswordStoreAndPrefsForTesting(
    PasswordStore* store,
    PrefService* prefs,
    const scoped_refptr<net::URLRequestContextGetter>& request_context);

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_HTTP_DATA_CLEANER_H_
