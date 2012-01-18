// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROTECTOR_MOCK_SETTING_CHANGE_H_
#define CHROME_BROWSER_PROTECTOR_MOCK_SETTING_CHANGE_H_
#pragma once

#include "base/compiler_specific.h"
#include "chrome/browser/protector/base_setting_change.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace protector {

class MockSettingChange : public BaseSettingChange {
 public:
  MockSettingChange();
  virtual ~MockSettingChange();

  virtual bool Init(Protector* protector) OVERRIDE;

  MOCK_METHOD1(MockInit, bool(Protector* protector));
  MOCK_METHOD0(Apply, void());
  MOCK_METHOD0(Discard, void());
  MOCK_METHOD0(Timeout, void());

  MOCK_CONST_METHOD0(GetBadgeIconID, int());
  MOCK_CONST_METHOD0(GetMenuItemIconID, int());
  MOCK_CONST_METHOD0(GetBubbleIconID, int());

  MOCK_CONST_METHOD0(GetBubbleTitle, string16());
  MOCK_CONST_METHOD0(GetBubbleMessage, string16());
  MOCK_CONST_METHOD0(GetApplyButtonText, string16());
  MOCK_CONST_METHOD0(GetDiscardButtonText, string16());
};

}  // namespace protector

#endif  // CHROME_BROWSER_PROTECTOR_MOCK_SETTING_CHANGE_H_
