// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// List of Group Policy Settings objects.

#include "build/build_config.h"
#include "chrome/browser/group_policy_settings.h"

namespace group_policy {

  // TODO(gwilson): The actual group policy settings need to be finalized.
  // Change this testing set to the true supported set.
  DEFINE_POLICY_STRING(Homepage, L"Preferences\\", L"Homepage");
  DEFINE_POLICY_BOOL(HomepageIsNewTabPage,
                     L"Preferences\\",
                     L"HomepageIsNewTabPage");
  DEFINE_POLICY_STRINGLIST(ChromeFrameDomainWhiteList,
                           L"ChromeFrameDomainWhiteList");

};  // namespace

