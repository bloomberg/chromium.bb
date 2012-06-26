// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTENT_SETTINGS_CONTENT_SETTINGS_OBSERVER_H_
#define CHROME_BROWSER_CONTENT_SETTINGS_CONTENT_SETTINGS_OBSERVER_H_

#include <string>

#include "chrome/common/content_settings_pattern.h"
#include "chrome/common/content_settings_types.h"

namespace content_settings {

class Observer {
 public:
  virtual void OnContentSettingChanged(
      const ContentSettingsPattern& primary_pattern,
      const ContentSettingsPattern& secondary_pattern,
      ContentSettingsType content_type,
      std::string resource_identifier) = 0;

 protected:
  virtual ~Observer() {}
};

}  // namespace content_settings

#endif  // CHROME_BROWSER_CONTENT_SETTINGS_CONTENT_SETTINGS_OBSERVER_H_
