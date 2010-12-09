// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ceee_module_util mocks.

#ifndef CEEE_IE_COMMON_MOCK_CEEE_MODULE_UTIL_H_
#define CEEE_IE_COMMON_MOCK_CEEE_MODULE_UTIL_H_

#include "ceee/ie/common/ceee_module_util.h"
#include "ceee/testing/utils/mock_static.h"

namespace testing {

MOCK_STATIC_CLASS_BEGIN(MockCeeeModuleUtils)
  MOCK_STATIC_INIT_BEGIN(MockCeeeModuleUtils)
    MOCK_STATIC_INIT2(ceee_module_util::SetOptionToolbandIsHidden,
                      SetOptionToolbandIsHidden);
    MOCK_STATIC_INIT2(ceee_module_util::GetOptionToolbandIsHidden,
                      GetOptionToolbandIsHidden);
    MOCK_STATIC_INIT2(ceee_module_util::SetOptionToolbandForceReposition,
                      SetOptionToolbandForceReposition);
    MOCK_STATIC_INIT2(ceee_module_util::GetOptionToolbandForceReposition,
                      GetOptionToolbandForceReposition);
    MOCK_STATIC_INIT2(ceee_module_util::SetOptionEnableWebProgressApis,
                      SetOptionEnableWebProgressApis);
    MOCK_STATIC_INIT2(ceee_module_util::GetOptionEnableWebProgressApis,
                      GetOptionEnableWebProgressApis);
    MOCK_STATIC_INIT2(ceee_module_util::SetIgnoreShowDWChanges,
                      SetIgnoreShowDWChanges);
    MOCK_STATIC_INIT2(ceee_module_util::GetIgnoreShowDWChanges,
                      GetIgnoreShowDWChanges);
    MOCK_STATIC_INIT2(ceee_module_util::GetExtensionPath,
                      GetExtensionPath);
  MOCK_STATIC_INIT_END()
  MOCK_STATIC1(void, , SetOptionToolbandIsHidden, bool);
  MOCK_STATIC0(bool, , GetOptionToolbandIsHidden);
  MOCK_STATIC1(void, , SetOptionToolbandForceReposition, bool);
  MOCK_STATIC0(bool, , GetOptionToolbandForceReposition);
  MOCK_STATIC1(void, , SetOptionEnableWebProgressApis, bool);
  MOCK_STATIC0(bool, , GetOptionEnableWebProgressApis);
  MOCK_STATIC1(void, , SetIgnoreShowDWChanges, bool);
  MOCK_STATIC0(bool, , GetIgnoreShowDWChanges);
  MOCK_STATIC0(std::wstring, , GetExtensionPath);
MOCK_STATIC_CLASS_END(MockCeeeModuleUtils)

}  // namespace testing

#endif  // CEEE_IE_COMMON_MOCK_CEEE_MODULE_UTIL_H_
