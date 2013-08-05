// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MANAGED_MODE_MANAGED_USERS_H_
#define CHROME_BROWSER_MANAGED_MODE_MANAGED_USERS_H_

// Compile-time check to make sure that we don't include managed user source
// files when ENABLE_MANAGED_USERS is not defined.
#if !defined(ENABLE_MANAGED_USERS)
#error "Managed users aren't enabled."
#endif

#endif  // CHROME_BROWSER_MANAGED_MODE_MANAGED_USERS_H_
