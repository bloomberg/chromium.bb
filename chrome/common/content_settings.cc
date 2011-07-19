// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/content_settings.h"

ContentSetting IntToContentSetting(int content_setting) {
  return ((content_setting < 0) ||
          (content_setting >= CONTENT_SETTING_NUM_SETTINGS)) ?
      CONTENT_SETTING_DEFAULT : static_cast<ContentSetting>(content_setting);
}
