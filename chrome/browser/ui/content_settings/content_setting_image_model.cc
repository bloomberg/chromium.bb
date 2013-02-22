// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/content_settings/content_setting_image_model.h"

#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/web_contents.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"

using content::WebContents;

class ContentSettingBlockedImageModel : public ContentSettingImageModel {
 public:
  explicit ContentSettingBlockedImageModel(
      ContentSettingsType content_settings_type);

  virtual void UpdateFromWebContents(WebContents* web_contents) OVERRIDE;
};

class ContentSettingGeolocationImageModel : public ContentSettingImageModel {
 public:
  ContentSettingGeolocationImageModel();

  virtual void UpdateFromWebContents(WebContents* web_contents) OVERRIDE;
};

// Image model for displaying media icons in the location bar.
class ContentSettingMediaImageModel : public ContentSettingImageModel {
 public:
  ContentSettingMediaImageModel();

  virtual void UpdateFromWebContents(WebContents* web_contents) OVERRIDE;
};

class ContentSettingRPHImageModel : public ContentSettingImageModel {
 public:
  ContentSettingRPHImageModel();

  virtual void UpdateFromWebContents(WebContents* web_contents) OVERRIDE;
};

class ContentSettingNotificationsImageModel : public ContentSettingImageModel {
 public:
  ContentSettingNotificationsImageModel();

  virtual void UpdateFromWebContents(WebContents* web_contents) OVERRIDE;
};

namespace {

struct ContentSettingsTypeIdEntry {
  ContentSettingsType type;
  int id;
};

int GetIdForContentType(const ContentSettingsTypeIdEntry* entries,
                        size_t num_entries,
                        ContentSettingsType type) {
  for (size_t i = 0; i < num_entries; ++i) {
    if (entries[i].type == type)
      return entries[i].id;
  }
  return 0;
}

}  // namespace

ContentSettingBlockedImageModel::ContentSettingBlockedImageModel(
    ContentSettingsType content_settings_type)
    : ContentSettingImageModel(content_settings_type) {
}

void ContentSettingBlockedImageModel::UpdateFromWebContents(
    WebContents* web_contents) {
  set_visible(false);
  if (!web_contents)
    return;

  static const ContentSettingsTypeIdEntry kBlockedIconIDs[] = {
    {CONTENT_SETTINGS_TYPE_COOKIES, IDR_BLOCKED_COOKIES},
    {CONTENT_SETTINGS_TYPE_IMAGES, IDR_BLOCKED_IMAGES},
    {CONTENT_SETTINGS_TYPE_JAVASCRIPT, IDR_BLOCKED_JAVASCRIPT},
    {CONTENT_SETTINGS_TYPE_PLUGINS, IDR_BLOCKED_PLUGINS},
    {CONTENT_SETTINGS_TYPE_POPUPS, IDR_BLOCKED_POPUPS},
    {CONTENT_SETTINGS_TYPE_MIXEDSCRIPT, IDR_BLOCKED_MIXED_CONTENT},
    {CONTENT_SETTINGS_TYPE_PPAPI_BROKER, IDR_BLOCKED_PPAPI_BROKER},
  };
  static const ContentSettingsTypeIdEntry kBlockedTooltipIDs[] = {
    {CONTENT_SETTINGS_TYPE_COOKIES, IDS_BLOCKED_COOKIES_TITLE},
    {CONTENT_SETTINGS_TYPE_IMAGES, IDS_BLOCKED_IMAGES_TITLE},
    {CONTENT_SETTINGS_TYPE_JAVASCRIPT, IDS_BLOCKED_JAVASCRIPT_TITLE},
    {CONTENT_SETTINGS_TYPE_PLUGINS, IDS_BLOCKED_PLUGINS_MESSAGE},
    {CONTENT_SETTINGS_TYPE_POPUPS, IDS_BLOCKED_POPUPS_TOOLTIP},
    {CONTENT_SETTINGS_TYPE_MIXEDSCRIPT,
        IDS_BLOCKED_DISPLAYING_INSECURE_CONTENT},
    {CONTENT_SETTINGS_TYPE_PPAPI_BROKER, IDS_BLOCKED_PPAPI_BROKER_TITLE},
  };
  static const ContentSettingsTypeIdEntry kBlockedExplanatoryTextIDs[] = {
    {CONTENT_SETTINGS_TYPE_POPUPS, IDS_BLOCKED_POPUPS_EXPLANATORY_TEXT},
  };

  ContentSettingsType type = get_content_settings_type();
  int icon_id = GetIdForContentType(
      kBlockedIconIDs, arraysize(kBlockedIconIDs), type);
  int tooltip_id = GetIdForContentType(
      kBlockedTooltipIDs, arraysize(kBlockedTooltipIDs), type);
  int explanation_id = GetIdForContentType(
      kBlockedExplanatoryTextIDs, arraysize(kBlockedExplanatoryTextIDs), type);

  // If a content type is blocked by default and was accessed, display the
  // accessed icon.
  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::FromWebContents(web_contents);
  if (!content_settings)
    return;
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  if (!content_settings->IsContentBlocked(get_content_settings_type())) {
    if (!content_settings->IsContentAllowed(get_content_settings_type()))
      return;

    // For cookies, only show the accessed bubble if cookies are blocked by
    // default.
    if (get_content_settings_type() == CONTENT_SETTINGS_TYPE_COOKIES &&
        (profile->GetHostContentSettingsMap()->
            GetDefaultContentSetting(CONTENT_SETTINGS_TYPE_COOKIES, NULL) !=
                CONTENT_SETTING_BLOCK))
      return;

    static const ContentSettingsTypeIdEntry kAccessedIconIDs[] = {
      {CONTENT_SETTINGS_TYPE_COOKIES, IDR_ACCESSED_COOKIES},
      {CONTENT_SETTINGS_TYPE_PPAPI_BROKER, IDR_BLOCKED_PPAPI_BROKER},
    };
    static const ContentSettingsTypeIdEntry kAccessedTooltipIDs[] = {
      {CONTENT_SETTINGS_TYPE_COOKIES, IDS_ACCESSED_COOKIES_TITLE},
      {CONTENT_SETTINGS_TYPE_PPAPI_BROKER, IDS_ALLOWED_PPAPI_BROKER_TITLE},
    };
    icon_id = GetIdForContentType(
        kAccessedIconIDs, arraysize(kAccessedIconIDs), type);
    tooltip_id = GetIdForContentType(
        kAccessedTooltipIDs, arraysize(kAccessedTooltipIDs), type);
    explanation_id = 0;
  }
  set_visible(true);
  DCHECK(icon_id);
  set_icon(icon_id);
  set_explanatory_string_id(explanation_id);
  DCHECK(tooltip_id);
  set_tooltip(l10n_util::GetStringUTF8(tooltip_id));
}

ContentSettingGeolocationImageModel::ContentSettingGeolocationImageModel()
    : ContentSettingImageModel(CONTENT_SETTINGS_TYPE_GEOLOCATION) {
}

void ContentSettingGeolocationImageModel::UpdateFromWebContents(
    WebContents* web_contents) {
  set_visible(false);
  if (!web_contents)
    return;
  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::FromWebContents(web_contents);
  if (!content_settings)
    return;
  const GeolocationSettingsState& settings_state = content_settings->
      geolocation_settings_state();
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

ContentSettingMediaImageModel::ContentSettingMediaImageModel()
    : ContentSettingImageModel(CONTENT_SETTINGS_TYPE_MEDIASTREAM) {
}

void ContentSettingMediaImageModel::UpdateFromWebContents(
    WebContents* web_contents) {
  set_visible(false);
  if (!web_contents)
    return;

  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::FromWebContents(web_contents);
  if (!content_settings)
    return;

  bool blocked =
      content_settings->IsContentBlocked(CONTENT_SETTINGS_TYPE_MEDIASTREAM);
  if (!blocked &&
      !content_settings->IsContentAllowed(get_content_settings_type()))
    return;

  set_tooltip(
      l10n_util::GetStringUTF8(blocked ?
          IDS_MEDIASTREAM_BLOCKED_TOOLTIP : IDS_MEDIASTREAM_ALLOWED_TOOLTIP));
  set_icon(blocked ? IDR_BLOCKED_MEDIA : IDR_ASK_MEDIA);
  set_visible(true);
}

ContentSettingRPHImageModel::ContentSettingRPHImageModel()
    : ContentSettingImageModel(
        CONTENT_SETTINGS_TYPE_PROTOCOL_HANDLERS) {
  set_icon(IDR_REGISTER_PROTOCOL_HANDLER_LOCATIONBAR_ICON);
  set_tooltip(l10n_util::GetStringUTF8(IDS_REGISTER_PROTOCOL_HANDLER_TOOLTIP));
}

void ContentSettingRPHImageModel::UpdateFromWebContents(
    WebContents* web_contents) {
  set_visible(false);
  if (!web_contents)
    return;

  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::FromWebContents(web_contents);
  if (!content_settings)
    return;
  if (content_settings->pending_protocol_handler().IsEmpty())
    return;

  set_visible(true);
}

ContentSettingNotificationsImageModel::ContentSettingNotificationsImageModel()
    : ContentSettingImageModel(CONTENT_SETTINGS_TYPE_NOTIFICATIONS) {
}

void ContentSettingNotificationsImageModel::UpdateFromWebContents(
    WebContents* web_contents) {
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
  switch (content_settings_type) {
    case CONTENT_SETTINGS_TYPE_GEOLOCATION:
      return new ContentSettingGeolocationImageModel();
    case CONTENT_SETTINGS_TYPE_NOTIFICATIONS:
      return new ContentSettingNotificationsImageModel();
    case CONTENT_SETTINGS_TYPE_PROTOCOL_HANDLERS:
      return new ContentSettingRPHImageModel();
    case CONTENT_SETTINGS_TYPE_MEDIASTREAM:
      return new ContentSettingMediaImageModel();
    default:
      return new ContentSettingBlockedImageModel(content_settings_type);
  }
}
