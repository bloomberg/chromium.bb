// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_setting_image_model.h"

#include "app/l10n_util.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"

class ContentSettingBlockedImageModel : public ContentSettingImageModel {
 public:
  explicit ContentSettingBlockedImageModel(
      ContentSettingsType content_settings_type);

  virtual void UpdateFromTabContents(const TabContents* tab_contents);

 private:
  static const int kBlockedIconIDs[];
  static const int kTooltipIDs[];
};

class ContentSettingGeolocationImageModel : public ContentSettingImageModel {
 public:
  ContentSettingGeolocationImageModel();

  virtual void UpdateFromTabContents(const TabContents* tab_contents);
};

const int ContentSettingBlockedImageModel::kBlockedIconIDs[] = {
    IDR_BLOCKED_COOKIES,
    IDR_BLOCKED_IMAGES,
    IDR_BLOCKED_JAVASCRIPT,
    IDR_BLOCKED_PLUGINS,
    IDR_BLOCKED_POPUPS,
};

const int ContentSettingBlockedImageModel::kTooltipIDs[] = {
  IDS_BLOCKED_COOKIES_TITLE,
  IDS_BLOCKED_IMAGES_TITLE,
  IDS_BLOCKED_JAVASCRIPT_TITLE,
  IDS_BLOCKED_PLUGINS_TITLE,
  IDS_BLOCKED_POPUPS_TOOLTIP,
};


ContentSettingBlockedImageModel::ContentSettingBlockedImageModel(
    ContentSettingsType content_settings_type)
    : ContentSettingImageModel(content_settings_type) {
}

void ContentSettingBlockedImageModel::UpdateFromTabContents(
    const TabContents* tab_contents) {
  if (!tab_contents ||
      !tab_contents->IsContentBlocked(get_content_settings_type())) {
    set_visible(false);
    return;
  }
  set_icon(kBlockedIconIDs[get_content_settings_type()]);
  set_tooltip(
      l10n_util::GetStringUTF8(kTooltipIDs[get_content_settings_type()]));
  set_visible(true);
}

ContentSettingGeolocationImageModel::ContentSettingGeolocationImageModel()
    : ContentSettingImageModel(CONTENT_SETTINGS_TYPE_GEOLOCATION) {
}

void ContentSettingGeolocationImageModel::UpdateFromTabContents(
    const TabContents* tab_contents) {
  if (!tab_contents) {
    set_visible(false);
    return;
  }
  const TabContents::GeolocationContentSettings& settings =
      tab_contents->geolocation_content_settings();
  if (settings.empty()) {
    set_visible(false);
    return;
  }
  set_visible(true);
  // If any embedded site has access the allowed icon takes priority over the
  // blocked icon.
  for (TabContents::GeolocationContentSettings::const_iterator it =
       settings.begin(); it != settings.end(); ++it ) {
    if (it->second == CONTENT_SETTING_ALLOW) {
      set_icon(IDR_GEOLOCATION_ALLOWED_LOCATIONBAR_ICON);
      set_tooltip(l10n_util::GetStringUTF8(IDS_GEOLOCATION_ALLOWED_TOOLTIP));
      return;
    }
  }
  set_icon(IDR_GEOLOCATION_DENIED_LOCATIONBAR_ICON);
  set_tooltip(l10n_util::GetStringUTF8(IDS_GEOLOCATION_BLOCKED_TOOLTIP));
}

ContentSettingImageModel::ContentSettingImageModel(
    ContentSettingsType content_settings_type)
    : content_settings_type_(content_settings_type),
      is_visible_(false),
      icon_(0) {
}

// static
ContentSettingImageModel*
    ContentSettingImageModel::CreateContentSettingImageModel(
    ContentSettingsType content_settings_type) {
  if (content_settings_type == CONTENT_SETTINGS_TYPE_GEOLOCATION)
    return new ContentSettingGeolocationImageModel();
  return new ContentSettingBlockedImageModel(content_settings_type);
}
