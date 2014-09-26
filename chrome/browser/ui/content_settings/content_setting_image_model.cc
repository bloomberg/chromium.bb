// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/content_settings/content_setting_image_model.h"

#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/web_contents.h"
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
  explicit ContentSettingMediaImageModel(ContentSettingsType type);

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

class ContentSettingMIDISysExImageModel : public ContentSettingImageModel {
 public:
  ContentSettingMIDISysExImageModel();

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
    {CONTENT_SETTINGS_TYPE_AUTOMATIC_DOWNLOADS, IDR_BLOCKED_DOWNLOADS},
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
    {CONTENT_SETTINGS_TYPE_AUTOMATIC_DOWNLOADS, IDS_BLOCKED_DOWNLOAD_TITLE},
  };
  static const ContentSettingsTypeIdEntry kBlockedExplanatoryTextIDs[] = {
    {CONTENT_SETTINGS_TYPE_POPUPS, IDS_BLOCKED_POPUPS_EXPLANATORY_TEXT},
    {CONTENT_SETTINGS_TYPE_PLUGINS, IDS_BLOCKED_PLUGIN_EXPLANATORY_TEXT},
    {CONTENT_SETTINGS_TYPE_AUTOMATIC_DOWNLOADS,
        IDS_BLOCKED_DOWNLOADS_EXPLANATION},
  };

  ContentSettingsType type = get_content_settings_type();
  int icon_id = GetIdForContentType(
      kBlockedIconIDs, arraysize(kBlockedIconIDs), type);
  int tooltip_id = GetIdForContentType(
      kBlockedTooltipIDs, arraysize(kBlockedTooltipIDs), type);
  int explanation_id = GetIdForContentType(
      kBlockedExplanatoryTextIDs, arraysize(kBlockedExplanatoryTextIDs), type);

  // For plugins, don't show the animated explanation unless the plugin was
  // blocked despite the user's content settings being set to allow it (e.g.
  // due to auto-blocking NPAPI plugins).
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  HostContentSettingsMap* map = profile->GetHostContentSettingsMap();
  if (type == CONTENT_SETTINGS_TYPE_PLUGINS) {
    GURL url = web_contents->GetURL();
    if (map->GetContentSetting(url, url, type, std::string()) !=
        CONTENT_SETTING_ALLOW)
      explanation_id = 0;
  }

  // If a content type is blocked by default and was accessed, display the
  // content blocked page action.
  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::FromWebContents(web_contents);
  if (!content_settings)
    return;
  if (!content_settings->IsContentBlocked(type)) {
    if (!content_settings->IsContentAllowed(type))
      return;

    // For cookies, only show the cookie blocked page action if cookies are
    // blocked by default.
    if (type == CONTENT_SETTINGS_TYPE_COOKIES &&
        (map->GetDefaultContentSetting(type, NULL) != CONTENT_SETTING_BLOCK))
      return;

    static const ContentSettingsTypeIdEntry kAccessedIconIDs[] = {
      {CONTENT_SETTINGS_TYPE_COOKIES, IDR_ACCESSED_COOKIES},
      {CONTENT_SETTINGS_TYPE_PPAPI_BROKER, IDR_BLOCKED_PPAPI_BROKER},
      {CONTENT_SETTINGS_TYPE_AUTOMATIC_DOWNLOADS, IDR_ALLOWED_DOWNLOADS},
    };
    static const ContentSettingsTypeIdEntry kAccessedTooltipIDs[] = {
      {CONTENT_SETTINGS_TYPE_COOKIES, IDS_ACCESSED_COOKIES_TITLE},
      {CONTENT_SETTINGS_TYPE_PPAPI_BROKER, IDS_ALLOWED_PPAPI_BROKER_TITLE},
      {CONTENT_SETTINGS_TYPE_AUTOMATIC_DOWNLOADS, IDS_ALLOWED_DOWNLOAD_TITLE},
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
  const ContentSettingsUsagesState& usages_state = content_settings->
      geolocation_usages_state();
  if (usages_state.state_map().empty())
    return;
  set_visible(true);

  // If any embedded site has access the allowed icon takes priority over the
  // blocked icon.
  unsigned int state_flags = 0;
  usages_state.GetDetailedInfo(NULL, &state_flags);
  bool allowed =
      !!(state_flags & ContentSettingsUsagesState::TABSTATE_HAS_ANY_ALLOWED);
  set_icon(allowed ? IDR_ALLOWED_LOCATION : IDR_BLOCKED_LOCATION);
  set_tooltip(l10n_util::GetStringUTF8(allowed ?
      IDS_GEOLOCATION_ALLOWED_TOOLTIP : IDS_GEOLOCATION_BLOCKED_TOOLTIP));
}

ContentSettingMediaImageModel::ContentSettingMediaImageModel(
    ContentSettingsType type)
    : ContentSettingImageModel(type) {
}

void ContentSettingMediaImageModel::UpdateFromWebContents(
    WebContents* web_contents) {
  set_visible(false);

  // As long as a single icon is used to display the status of the camera and
  // microphone usage only display an icon for the
  // CONTENT_SETTINGS_TYPE_MEDIASTREAM. Don't display anything for
  // CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC,
  // CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA.
  // FIXME: Remove this hack and either display a two omnibox icons (one for
  // camera and one for microphone), or don't create one image model per
  // content type but per icon to display. The later is probably the right
  // thing to do, bebacuse this also allows to add more content settings type
  // for which no omnibox icon exists.
  if (get_content_settings_type() == CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC ||
      get_content_settings_type() == CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA) {
    return;
  }

  // The ContentSettingMediaImageModel must not be used with a content type
  // other then: CONTENT_SETTINGS_TYPE_MEDIASTREAM,
  // CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC,
  // CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA.
  DCHECK_EQ(get_content_settings_type(), CONTENT_SETTINGS_TYPE_MEDIASTREAM);

  if (!web_contents)
    return;

  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::FromWebContents(web_contents);
  if (!content_settings)
    return;
  TabSpecificContentSettings::MicrophoneCameraState state =
      content_settings->GetMicrophoneCameraState();

  // If neither the microphone nor the camera stream was accessed then no icon
  // is displayed in the omnibox.
  if (state == TabSpecificContentSettings::MICROPHONE_CAMERA_NOT_ACCESSED)
    return;

  bool is_mic = (state & TabSpecificContentSettings::MICROPHONE_ACCESSED) != 0;
  bool is_cam = (state & TabSpecificContentSettings::CAMERA_ACCESSED) != 0;
  DCHECK(is_mic || is_cam);

  int id = IDS_CAMERA_BLOCKED;
  if (state & (TabSpecificContentSettings::MICROPHONE_BLOCKED |
               TabSpecificContentSettings::CAMERA_BLOCKED)) {
    set_icon(IDR_BLOCKED_MEDIA);
    if (is_mic)
      id = is_cam ? IDS_MICROPHONE_CAMERA_BLOCKED : IDS_MICROPHONE_BLOCKED;
  } else {
    set_icon(IDR_ASK_MEDIA);
    id = IDS_CAMERA_ACCESSED;
    if (is_mic)
      id = is_cam ? IDS_MICROPHONE_CAMERA_ALLOWED : IDS_MICROPHONE_ACCESSED;
  }
  set_tooltip(l10n_util::GetStringUTF8(id));
  set_visible(true);
}

ContentSettingRPHImageModel::ContentSettingRPHImageModel()
    : ContentSettingImageModel(
        CONTENT_SETTINGS_TYPE_PROTOCOL_HANDLERS) {
  set_icon(IDR_REGISTER_PROTOCOL_HANDLER);
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

ContentSettingMIDISysExImageModel::ContentSettingMIDISysExImageModel()
    : ContentSettingImageModel(CONTENT_SETTINGS_TYPE_MIDI_SYSEX) {
}

void ContentSettingMIDISysExImageModel::UpdateFromWebContents(
    WebContents* web_contents) {
  set_visible(false);
  if (!web_contents)
    return;
  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::FromWebContents(web_contents);
  if (!content_settings)
    return;
  const ContentSettingsUsagesState& usages_state =
      content_settings->midi_usages_state();
  if (usages_state.state_map().empty())
    return;
  set_visible(true);

  // If any embedded site has access the allowed icon takes priority over the
  // blocked icon.
  unsigned int state_flags = 0;
  usages_state.GetDetailedInfo(NULL, &state_flags);
  bool allowed =
      !!(state_flags & ContentSettingsUsagesState::TABSTATE_HAS_ANY_ALLOWED);
  set_icon(allowed ? IDR_ALLOWED_MIDI_SYSEX : IDR_BLOCKED_MIDI_SYSEX);
  set_tooltip(l10n_util::GetStringUTF8(allowed ?
      IDS_MIDI_SYSEX_ALLOWED_TOOLTIP : IDS_MIDI_SYSEX_BLOCKED_TOOLTIP));
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
    case CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC:
    case CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA:
      return new ContentSettingMediaImageModel(content_settings_type);
    case CONTENT_SETTINGS_TYPE_MIDI_SYSEX:
      return new ContentSettingMIDISysExImageModel();
    default:
      return new ContentSettingBlockedImageModel(content_settings_type);
  }
}
