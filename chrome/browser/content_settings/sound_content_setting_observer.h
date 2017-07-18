// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTENT_SETTINGS_SOUND_CONTENT_SETTING_OBSERVER_H_
#define CHROME_BROWSER_CONTENT_SETTINGS_SOUND_CONTENT_SETTING_OBSERVER_H_

#include "base/scoped_observer.h"
#include "components/content_settings/core/browser/content_settings_observer.h"
#include "components/content_settings/core/common/content_settings.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

class HostContentSettingsMap;

class SoundContentSettingObserver
    : public content::WebContentsObserver,
      public content::WebContentsUserData<SoundContentSettingObserver>,
      public content_settings::Observer {
 public:
  ~SoundContentSettingObserver() override;

  // content::WebContentsObserver implementation.
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

  // content_settings::Observer implementation.
  void OnContentSettingChanged(const ContentSettingsPattern& primary_pattern,
                               const ContentSettingsPattern& secondary_pattern,
                               ContentSettingsType content_type,
                               std::string resource_identifier) override;

 private:
  explicit SoundContentSettingObserver(content::WebContents* web_contents);
  friend class content::WebContentsUserData<SoundContentSettingObserver>;

  void MuteOrUnmuteIfNecessary();
  ContentSetting GetCurrentContentSetting();

  HostContentSettingsMap* host_content_settings_map_;

  ScopedObserver<HostContentSettingsMap, content_settings::Observer> observer_;

  DISALLOW_COPY_AND_ASSIGN(SoundContentSettingObserver);
};

#endif  // CHROME_BROWSER_CONTENT_SETTINGS_SOUND_CONTENT_SETTING_OBSERVER_H_
