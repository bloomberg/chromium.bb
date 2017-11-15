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
  // The reason why the site was muted. This is logged to UKM, so add new values
  // at the end.
  enum MuteReason {
    kSiteException = 0,  // Muted due to an explicit block exception.
    kMuteByDefault = 1,  // Muted due to the default sound setting being set to
                         // block.
  };

  ~SoundContentSettingObserver() override;

  // content::WebContentsObserver implementation.
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

  // content_settings::Observer implementation.
  void OnContentSettingChanged(const ContentSettingsPattern& primary_pattern,
                               const ContentSettingsPattern& secondary_pattern,
                               ContentSettingsType content_type,
                               std::string resource_identifier) override;

  // This method is called by the WebContentsDelegate to indicate that the audio
  // state of the WebContents has changed.
  void OnAudioStateChanged(bool is_audible);

 private:
  explicit SoundContentSettingObserver(content::WebContents* web_contents);
  friend class content::WebContentsUserData<SoundContentSettingObserver>;

  void MuteOrUnmuteIfNecessary();
  ContentSetting GetCurrentContentSetting();

  // Records SiteMuted UKM event if site is muted and sound is playing.
  void CheckSoundBlocked(bool is_audible);

  // Record a UKM event that audio was blocked on the page.
  void RecordSiteMutedUKM();

  // Determine the reason why audio was blocked on the page.
  MuteReason GetSiteMutedReason();

  // True if we have already logged a SiteMuted UKM event since last navigation.
  bool logged_site_muted_ukm_;

  HostContentSettingsMap* host_content_settings_map_;

  ScopedObserver<HostContentSettingsMap, content_settings::Observer> observer_;

  DISALLOW_COPY_AND_ASSIGN(SoundContentSettingObserver);
};

#endif  // CHROME_BROWSER_CONTENT_SETTINGS_SOUND_CONTENT_SETTING_OBSERVER_H_
