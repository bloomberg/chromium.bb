// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/protector/mock_setting_change.h"

#include "base/utf_string_conversions.h"
#include "grit/theme_resources.h"
#include "grit/theme_resources_standard.h"

using ::testing::Return;

namespace protector {

MockSettingChange::MockSettingChange() {
  ON_CALL(*this, GetBadgeIconID()).WillByDefault(Return(IDR_UPDATE_BADGE4));
  ON_CALL(*this, GetMenuItemIconID()).WillByDefault(Return(IDR_UPDATE_MENU4));
  ON_CALL(*this, GetBubbleIconID()).WillByDefault(Return(IDR_INPUT_ALERT));

  ON_CALL(*this, GetBubbleTitle()).
      WillByDefault(Return(UTF8ToUTF16("Title")));
  ON_CALL(*this, GetBubbleMessage()).
      WillByDefault(Return(UTF8ToUTF16("Message")));
  ON_CALL(*this, GetApplyButtonText()).
      WillByDefault(Return(UTF8ToUTF16("Apply")));
  ON_CALL(*this, GetDiscardButtonText()).
      WillByDefault(Return(UTF8ToUTF16("Discard")));

  ON_CALL(*this, GetApplyDisplayName()).
      WillByDefault(Return(
          DisplayName(kDefaultNamePriority, UTF8ToUTF16("new settings"))));

  ON_CALL(*this, GetNewSettingURL()).WillByDefault(Return(GURL()));
  ON_CALL(*this, CanBeMerged()).WillByDefault(Return(false));

  ON_CALL(*this, IsUserVisible()).WillByDefault(Return(true));
}

MockSettingChange::~MockSettingChange() {
}

bool MockSettingChange::Init(Profile* profile) {
  if (!BaseSettingChange::Init(profile))
    return false;
  return MockInit(profile);
}

}  // namespace protector
