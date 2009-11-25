// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GROUP_POLICY_SETTINGS_H_
#define CHROME_BROWSER_GROUP_POLICY_SETTINGS_H_

#include "chrome/browser/group_policy.h"

#define DECLARE_POLICY(name, type) extern const type gp##name
#define DECLARE_POLICY_DWORD(name) DECLARE_POLICY(name, DWORDSetting)
#define DECLARE_POLICY_BOOL(name) DECLARE_POLICY(name, BoolSetting)
#define DECLARE_POLICY_STRING(name) DECLARE_POLICY(name, StringSetting)
#define DECLARE_POLICY_STRINGLIST(name) DECLARE_POLICY(name, StringListSetting)

#define DEFINE_POLICY(name, type, key, value, storage, combine) \
    const type gp##name(key, value, storage, combine)
#define DEFINE_POLICY_DWORD(name, key, value) \
    DEFINE_POLICY(name, DWORDSetting, key, \
    value, POLICYSTORAGE_SINGLEVALUE, POLICYCOMBINE_PREFERMACHINE)
#define DEFINE_POLICY_BOOL(name, key, value) \
    DEFINE_POLICY(name, BoolSetting, key, value, \
    POLICYSTORAGE_SINGLEVALUE, POLICYCOMBINE_LOGICALOR)
#define DEFINE_POLICY_STRING(name, key, value) \
    DEFINE_POLICY(name, StringSetting, key, value, \
    POLICYSTORAGE_SINGLEVALUE, POLICYCOMBINE_PREFERMACHINE)
#define DEFINE_POLICY_STRINGLIST(name, key) \
    DEFINE_POLICY(name, StringListSetting, key, L"", \
    POLICYSTORAGE_CONCATSUBKEYS, POLICYCOMBINE_CONCATENATE)

namespace group_policy {

typedef Setting<DWORD> DWORDSetting;
typedef Setting<bool>  BoolSetting;
typedef Setting<std::wstring> StringSetting;
typedef Setting<ListValue> StringListSetting;

// Extern declarations of all policy settings.  Policies should always
// be accessed via the GroupPolicySetting object rather than directly
// through the registry, to ensure the priorities and combination flags
// are respected.
// TODO(gwilson): The actual group policy settings need to be finalized.
// Change this testing set to the true supported set.
DECLARE_POLICY_STRING(Homepage);
DECLARE_POLICY_BOOL(HomepageIsNewTabPage);
DECLARE_POLICY_STRINGLIST(ChromeFrameDomainWhiteList);

}  // namespace

#endif  // CHROME_BROWSER_GROUP_POLICY_SETTINGS_H_

