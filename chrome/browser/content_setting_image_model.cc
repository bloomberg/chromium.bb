// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_setting_image_model.h"

#include "base/command_line.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "content/browser/tab_contents/tab_contents.h"
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

  virtual void UpdateFromTabContents(TabContents* tab_contents) OVERRIDE;
};

class ContentSettingNotificationsImageModel : public ContentSettingImageModel {
 public:
  ContentSettingNotificationsImageModel();

  virtual void UpdateFromTabContents(TabContents* tab_contents) OVERRIDE;
};

class ContentSettingPrerenderImageModel : public ContentSettingImageModel {
 public:
  ContentSettingPrerenderImageModel();

  virtual void UpdateFromTabContents(TabContents* tab_contents) OVERRIDE;
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
  set_visible(false);
  if (!tab_contents)
    return;

  const int* icon_ids = kBlockedIconIDs;
  const int* tooltip_ids = kBlockedTooltipIDs;
  const int* explanatory_string_ids = kBlockedExplanatoryTextIDs;
  // If a content type is blocked by default and was accessed, display the
  // accessed icon.
  TabSpecificContentSettings* content_settings =
      tab_contents->GetTabSpecificContentSettings();
  if (!content_settings->IsContentBlocked(get_content_settings_type())) {
    if (!content_settings->IsContentAccessed(get_content_settings_type()) ||
        (tab_contents->profile()->GetHostContentSettingsMap()->
            GetDefaultContentSetting(get_content_settings_type()) !=
                CONTENT_SETTING_BLOCK))
      return;
    icon_ids = kAccessedIconIDs;
    tooltip_ids = kAccessedTooltipIDs;
    explanatory_string_ids = kAccessedExplanatoryTextIDs;
  }
  set_visible(true);
  set_icon(icon_ids[get_content_settings_type()]);
  set_explanatory_string_id(
      explanatory_string_ids[get_content_settings_type()]);
  set_tooltip(
      l10n_util::GetStringUTF8(tooltip_ids[get_content_settings_type()]));
}

ContentSettingGeolocationImageModel::ContentSettingGeolocationImageModel()
    : ContentSettingImageModel(CONTENT_SETTINGS_TYPE_GEOLOCATION) {
}

void ContentSettingGeolocationImageModel::UpdateFromTabContents(
    TabContents* tab_contents) {
  set_visible(false);
  if (!tab_contents)
    return;
  const GeolocationSettingsState& settings_state = tab_contents->
      GetTabSpecificContentSettings()->geolocation_settings_state();
  if (settings_state.state_map().empty())
    return;
  set_visible(true);

  // If any embedded site has access the allowed icon takes priority over the
  // blocked icon.
  unsigned int tab_state_flags = 0;
  settings_state.GetDetailedInfo(NULL, &tab_state_flags);
  bool allowed =
      !!(tab_state_flags & GeolocationSettingsState::TABSTATE_HAS_ANY_ALLOWED);
  set_icon(allowed ? IDR_GEOLOCATION_ALLOWED_LOCATIONBAR_ICON :
      IDR_GEOLOCATION_DENIED_LOCATIONBAR_ICON);
  set_tooltip(l10n_util::GetStringUTF8(allowed ?
      IDS_GEOLOCATION_ALLOWED_TOOLTIP : IDS_GEOLOCATION_BLOCKED_TOOLTIP));
}

ContentSettingNotificationsImageModel::ContentSettingNotificationsImageModel()
    : ContentSettingImageModel(CONTENT_SETTINGS_TYPE_NOTIFICATIONS) {
}

void ContentSettingNotificationsImageModel::UpdateFromTabContents(
    TabContents* tab_contents) {
  // Notifications do not have a bubble.
  set_visible(false);
}

ContentSettingPrerenderImageModel::ContentSettingPrerenderImageModel()
    : ContentSettingImageModel(CONTENT_SETTINGS_TYPE_PRERENDER) {
  set_tooltip(l10n_util::GetStringUTF8(IDS_PRERENDER_SUCCEED_TOOLTIP));
  set_icon(IDR_PRERENDER_SUCCEED_ICON);
}

void ContentSettingPrerenderImageModel::UpdateFromTabContents(
    TabContents* tab_contents) {
  bool visibility = false;
  if (tab_contents) {
    prerender::PrerenderManager* pm =
        tab_contents->profile()->GetPrerenderManager();
    if (pm && pm->IsTabContentsPrerendered(tab_contents))
      visibility = true;
  }
  set_visible(visibility);
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
  switch (content_settings_type) {
    case CONTENT_SETTINGS_TYPE_GEOLOCATION:
      return new ContentSettingGeolocationImageModel();
    case CONTENT_SETTINGS_TYPE_NOTIFICATIONS:
      return new ContentSettingNotificationsImageModel();
    case CONTENT_SETTINGS_TYPE_PRERENDER:
      return new ContentSettingPrerenderImageModel();
    default:
      return new ContentSettingBlockedImageModel(content_settings_type);
  }
}
