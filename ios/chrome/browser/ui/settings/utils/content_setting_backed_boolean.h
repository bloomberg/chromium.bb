// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_UTILS_CONTENT_SETTING_BACKED_BOOLEAN_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_UTILS_CONTENT_SETTING_BACKED_BOOLEAN_H_

#import <Foundation/Foundation.h>

#include "components/content_settings/core/common/content_settings_types.h"
#import "ios/chrome/browser/ui/settings/utils/observable_boolean.h"

class HostContentSettingsMap;

// An observable boolean backed by a setting from a HostContentSettingsMap.
@interface ContentSettingBackedBoolean : NSObject<ObservableBoolean>

// Returns a ContentSettingBackedBoolean backed by |settingID| from
// |settingsMap|. |inverted| specifies that the ON state of the boolean value
// corresponds to the OFF state of the content setting. For example, a boolean
// value for "disable popups" that corresponds to a model object that allows
// popups.
- (instancetype)initWithHostContentSettingsMap:
                    (HostContentSettingsMap*)settingsMap
                                     settingID:(ContentSettingsType)settingID
                                      inverted:(BOOL)inverted
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_UTILS_CONTENT_SETTING_BACKED_BOOLEAN_H_
