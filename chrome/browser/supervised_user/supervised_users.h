// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USERS_H_
#define CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USERS_H_

// Compile-time check to make sure that we don't include supervised user source
// files when ENABLE_MANAGED_USERS is not defined.
#if !defined(ENABLE_MANAGED_USERS)
#error "Supervised users aren't enabled."
#endif

#endif  // CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USERS_H_
