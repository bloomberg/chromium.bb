// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTENT_SETTINGS_MOCK_CONTENT_SETTINGS_PROVIDER_H_
#define CHROME_BROWSER_CONTENT_SETTINGS_MOCK_CONTENT_SETTINGS_PROVIDER_H_
#pragma once

#include "base/basictypes.h"
#include "chrome/browser/content_settings/content_settings_provider.h"

class MockContentSettingsProvider : public DefaultContentSettingsProvider {
 public:
  // Create a content settings provider that provides a given setting for a
  // given type.
  MockContentSettingsProvider(ContentSettingsType content_type,
                              ContentSetting setting,
                              bool is_managed,
                              bool can_override);
  virtual ~MockContentSettingsProvider();

  // ContentSettingsProviderInterface implementation.
  virtual bool CanProvideDefaultSetting(ContentSettingsType content_type) const;
  virtual ContentSetting ProvideDefaultSetting(
      ContentSettingsType content_type) const;
  virtual void UpdateDefaultSetting(ContentSettingsType content_type,
                                    ContentSetting setting);
  virtual void ResetToDefaults();
  virtual bool DefaultSettingIsManaged(ContentSettingsType content_type) const;

 private:
  ContentSettingsType content_type_;
  ContentSetting setting_;
  bool is_managed_;
  bool can_override_;

  DISALLOW_COPY_AND_ASSIGN(MockContentSettingsProvider);
};

#endif  // CHROME_BROWSER_CONTENT_SETTINGS_MOCK_CONTENT_SETTINGS_PROVIDER_H_
