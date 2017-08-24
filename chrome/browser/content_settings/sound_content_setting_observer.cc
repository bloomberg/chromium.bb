// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_settings/sound_content_setting_observer.h"

#include "build/build_config.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "content/public/browser/navigation_handle.h"

#if !defined(OS_ANDROID)
#include "chrome/browser/ui/tabs/tab_utils.h"
#endif

DEFINE_WEB_CONTENTS_USER_DATA_KEY(SoundContentSettingObserver);

SoundContentSettingObserver::SoundContentSettingObserver(
    content::WebContents* contents)
    : content::WebContentsObserver(contents), observer_(this) {
  host_content_settings_map_ = HostContentSettingsMapFactory::GetForProfile(
      Profile::FromBrowserContext(web_contents()->GetBrowserContext()));
  observer_.Add(host_content_settings_map_);
}

SoundContentSettingObserver::~SoundContentSettingObserver() = default;

void SoundContentSettingObserver::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (navigation_handle->IsInMainFrame() && navigation_handle->HasCommitted() &&
      !navigation_handle->IsSameDocument()) {
    MuteOrUnmuteIfNecessary();
  }
}

void SoundContentSettingObserver::OnContentSettingChanged(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsType content_type,
    std::string resource_identifier) {
  if (content_type == CONTENT_SETTINGS_TYPE_SOUND)
    MuteOrUnmuteIfNecessary();
}

void SoundContentSettingObserver::MuteOrUnmuteIfNecessary() {
  bool mute = GetCurrentContentSetting() == CONTENT_SETTING_BLOCK;

// TabMutedReason does not exist on Android.
#if defined(OS_ANDROID)
  web_contents()->SetAudioMuted(mute);
#else
  // We don't want to overwrite TabMutedReason with no change.
  if (mute == web_contents()->IsAudioMuted())
    return;

  TabMutedReason reason = chrome::GetTabAudioMutedReason(web_contents());

  // Do not unmute if we're muted due to media capture.
  if (!mute && reason == TabMutedReason::MEDIA_CAPTURE)
    return;

  // Do not override the decisions of an extension.
  if (reason == TabMutedReason::EXTENSION)
    return;

  chrome::SetTabAudioMuted(web_contents(), mute,
                           TabMutedReason::CONTENT_SETTING, std::string());
#endif  // defined(OS_ANDROID)
}

ContentSetting SoundContentSettingObserver::GetCurrentContentSetting() {
  GURL url = web_contents()->GetLastCommittedURL();
  return host_content_settings_map_->GetContentSetting(
      url, url, CONTENT_SETTINGS_TYPE_SOUND, std::string());
}
