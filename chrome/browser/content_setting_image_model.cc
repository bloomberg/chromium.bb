// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_setting_image_model.h"

#include "base/command_line.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"

class ContentSettingBlockedImageModel : public ContentSettingImageModel {
 public:
  explicit ContentSettingBlockedImageModel(
      ContentSettingsType content_settings_type);

  virtual void UpdateFromTabContents(TabContents* tab_contents);

 private:
  static const int kAccessedIconIDs[];
  static const int kBlockedIconIDs[];
  static const int kBlockedExplanatoryTextIDs[];
  static const int kAccessedExplanatoryTextIDs[];
  static const int kAccessedTooltipIDs[];
  static const int kBlockedTooltipIDs[];
};

class ContentSettingGeolocationImageModel : public ContentSettingImageModel {
 public:
  ContentSettingGeolocationImageModel();

  virtual void UpdateFromTabContents(TabContents* tab_contents);
};

class ContentSettingNotificationsImageModel : public ContentSettingImageModel {
 public:
  ContentSettingNotificationsImageModel();

  virtual void UpdateFromTabContents(TabContents* tab_contents);
};

const int ContentSettingBlockedImageModel::kBlockedIconIDs[] = {
    IDR_BLOCKED_COOKIES,
    IDR_BLOCKED_IMAGES,
    IDR_BLOCKED_JAVASCRIPT,
    IDR_BLOCKED_PLUGINS,
    IDR_BLOCKED_POPUPS,
};

const int ContentSettingBlockedImageModel::kAccessedIconIDs[] = {
    IDR_ACCESSED_COOKIES,
    0,
    0,
    0,
    0,
};

const int ContentSettingBlockedImageModel::kBlockedExplanatoryTextIDs[] = {
    0,
    0,
    0,
    0,
    IDS_BLOCKED_POPUPS_EXPLANATORY_TEXT,
};

const int ContentSettingBlockedImageModel::kAccessedExplanatoryTextIDs[] = {
    0,
    0,
    0,
    0,
    0,
};


const int ContentSettingBlockedImageModel::kBlockedTooltipIDs[] = {
    IDS_BLOCKED_COOKIES_TITLE,
    IDS_BLOCKED_IMAGES_TITLE,
    IDS_BLOCKED_JAVASCRIPT_TITLE,
    IDS_BLOCKED_PLUGINS_MESSAGE,
    IDS_BLOCKED_POPUPS_TOOLTIP,
};

const int ContentSettingBlockedImageModel::kAccessedTooltipIDs[] = {
    IDS_ACCESSED_COOKIES_TITLE,
    0,
    0,
    0,
    0,
};

ContentSettingBlockedImageModel::ContentSettingBlockedImageModel(
    ContentSettingsType content_settings_type)
    : ContentSettingImageModel(content_settings_type) {
}

void ContentSettingBlockedImageModel::UpdateFromTabContents(
    TabContents* tab_contents) {
  TabSpecificContentSettings* content_settings = tab_contents ?
      tab_contents->GetTabSpecificContentSettings() : NULL;
  const int* icon_ids;
  const int* tooltip_ids;
  const int* explanatory_string_ids;

  if (!content_settings) {
    set_visible(false);
    return;
  }
  if (content_settings->IsContentBlocked(get_content_settings_type())) {
    icon_ids = kBlockedIconIDs;
    tooltip_ids = kBlockedTooltipIDs;
    explanatory_string_ids = kBlockedExplanatoryTextIDs;
  } else if (tab_contents->profile()->GetHostContentSettingsMap()->
                 GetDefaultContentSetting(get_content_settings_type()) ==
                 CONTENT_SETTING_BLOCK &&
             content_settings->IsContentAccessed(get_content_settings_type())) {
    // If a content type is blocked by default and was accessed, display the
    // accessed icon.
    icon_ids = kAccessedIconIDs;
    tooltip_ids = kAccessedTooltipIDs;
    explanatory_string_ids = kAccessedExplanatoryTextIDs;
  } else {
    set_visible(false);
    return;
  }
  set_icon(icon_ids[get_content_settings_type()]);
  set_explanatory_string_id(
      explanatory_string_ids[get_content_settings_type()]);
  set_tooltip(
      l10n_util::GetStringUTF8(tooltip_ids[get_content_settings_type()]));
  set_visible(true);
}

ContentSettingGeolocationImageModel::ContentSettingGeolocationImageModel()
    : ContentSettingImageModel(CONTENT_SETTINGS_TYPE_GEOLOCATION) {
}

void ContentSettingGeolocationImageModel::UpdateFromTabContents(
    TabContents* tab_contents) {
  if (!tab_contents) {
    set_visible(false);
    return;
  }
  TabSpecificContentSettings* content_settings =
      tab_contents->GetTabSpecificContentSettings();
  const GeolocationSettingsState& settings_state =
      content_settings->geolocation_settings_state();
  if (settings_state.state_map().empty()) {
    set_visible(false);
    return;
  }
  set_visible(true);
  unsigned int tab_state_flags = 0;
  settings_state.GetDetailedInfo(NULL, &tab_state_flags);
  // If any embedded site has access the allowed icon takes priority over the
  // blocked icon.
  if (tab_state_flags & GeolocationSettingsState::TABSTATE_HAS_ANY_ALLOWED) {
    set_icon(IDR_GEOLOCATION_ALLOWED_LOCATIONBAR_ICON);
    set_tooltip(l10n_util::GetStringUTF8(IDS_GEOLOCATION_ALLOWED_TOOLTIP));
    return;
  }
  set_icon(IDR_GEOLOCATION_DENIED_LOCATIONBAR_ICON);
  set_tooltip(l10n_util::GetStringUTF8(IDS_GEOLOCATION_BLOCKED_TOOLTIP));
}

ContentSettingNotificationsImageModel::ContentSettingNotificationsImageModel()
    : ContentSettingImageModel(CONTENT_SETTINGS_TYPE_NOTIFICATIONS) {
}

void ContentSettingNotificationsImageModel::UpdateFromTabContents(
    TabContents* tab_contents) {
  // Notifications do not have a bubble.
  set_visible(false);
}

ContentSettingImageModel::ContentSettingImageModel(
    ContentSettingsType content_settings_type)
    : content_settings_type_(content_settings_type),
      is_visible_(false),
      icon_(0),
      explanatory_string_id_(0) {
}

// static
ContentSettingImageModel*
    ContentSettingImageModel::CreateContentSettingImageModel(
    ContentSettingsType content_settings_type) {
  if (content_settings_type == CONTENT_SETTINGS_TYPE_GEOLOCATION)
    return new ContentSettingGeolocationImageModel();
  if (content_settings_type == CONTENT_SETTINGS_TYPE_NOTIFICATIONS)
    return new ContentSettingNotificationsImageModel();
  return new ContentSettingBlockedImageModel(content_settings_type);
}
