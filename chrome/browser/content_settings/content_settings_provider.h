// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Interface for objects providing content setting rules.

#ifndef CHROME_BROWSER_CONTENT_SETTINGS_CONTENT_SETTINGS_PROVIDER_H_
#define CHROME_BROWSER_CONTENT_SETTINGS_CONTENT_SETTINGS_PROVIDER_H_
#pragma once

#include "chrome/common/content_settings.h"

class DefaultContentSettingsProvider {
 public:
  virtual ~DefaultContentSettingsProvider() {}

  // True if this provider can provide a default setting for the |content_type|.
  virtual bool CanProvideDefaultSetting(
      ContentSettingsType content_type) const = 0;

  // Returns the default content setting this provider has for the given
  // |content_type|, or CONTENT_SETTING_DEFAULT if nothing be provided for this
  // type.
  virtual ContentSetting ProvideDefaultSetting(
      ContentSettingsType content_type) const = 0;

  // Notifies the provider that the host content settings map would like to
  // update the default setting for the given |content_type|. The provider may
  // ignore this.
  virtual void UpdateDefaultSetting(ContentSettingsType content_type,
                                    ContentSetting setting) = 0;

  // Resets the state of the provider to the default.
  virtual void ResetToDefaults() = 0;

  // True if the default setting for the |content_type| is policy managed, i.e.,
  // there shouldn't be any UI shown to modify this setting.
  virtual bool DefaultSettingIsManaged(
      ContentSettingsType content_type) const = 0;
};

#endif  // CHROME_BROWSER_CONTENT_SETTINGS_CONTENT_SETTINGS_PROVIDER_H_
