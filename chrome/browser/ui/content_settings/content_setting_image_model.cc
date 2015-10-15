// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/content_settings/content_setting_image_model.h"

#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/grit/generated_resources.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "content/public/browser/web_contents.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/material_design/material_design_controller.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/vector_icons_public.h"

#if !defined(OS_MACOSX)
#include "ui/gfx/color_palette.h"
#include "ui/gfx/paint_vector_icon.h"
#endif

using content::WebContents;

namespace {

bool UseVectorGraphics() {
#if defined(OS_MACOSX)
  return false;
#else
  return ui::MaterialDesignController::IsModeMaterial();
#endif
}

#if !defined(OS_MACOSX)
// Gets a vector icon badged with |badge|.
gfx::Image GetIcon(gfx::VectorIconId id, gfx::VectorIconId badge) {
  return gfx::Image(
      gfx::CreateVectorIconWithBadge(id, 16, gfx::kChromeIconGrey, badge));
}
#endif

}  // namespace

class ContentSettingBlockedImageModel : public ContentSettingImageModel {
 public:
  explicit ContentSettingBlockedImageModel(
      ContentSettingsType content_settings_type);

  void UpdateFromWebContents(WebContents* web_contents) override;
};

class ContentSettingGeolocationImageModel : public ContentSettingImageModel {
 public:
  ContentSettingGeolocationImageModel();

  void UpdateFromWebContents(WebContents* web_contents) override;
};

// Image model for displaying media icons in the location bar.
class ContentSettingMediaImageModel : public ContentSettingImageModel {
 public:
  explicit ContentSettingMediaImageModel(ContentSettingsType type);

  void UpdateFromWebContents(WebContents* web_contents) override;
};

class ContentSettingRPHImageModel : public ContentSettingImageModel {
 public:
  ContentSettingRPHImageModel();

  void UpdateFromWebContents(WebContents* web_contents) override;
};

class ContentSettingNotificationsImageModel : public ContentSettingImageModel {
 public:
  ContentSettingNotificationsImageModel();

  void UpdateFromWebContents(WebContents* web_contents) override;
};

class ContentSettingMIDISysExImageModel : public ContentSettingImageModel {
 public:
  ContentSettingMIDISysExImageModel();

  void UpdateFromWebContents(WebContents* web_contents) override;
};

namespace {

struct ContentSettingsImageDetails {
  ContentSettingsType type;
  int blocked_icon_id;
  gfx::VectorIconId vector_icon_id;
  int blocked_tooltip_id;
  int blocked_explanatory_text_id;
  int accessed_icon_id;
  int accessed_tooltip_id;
};

static const ContentSettingsImageDetails kImageDetails[] = {
    {CONTENT_SETTINGS_TYPE_COOKIES,
     IDR_BLOCKED_COOKIES,
     gfx::VectorIconId::COOKIE,
     IDS_BLOCKED_COOKIES_TITLE,
     0,
     IDR_ACCESSED_COOKIES,
     IDS_ACCESSED_COOKIES_TITLE},
    {CONTENT_SETTINGS_TYPE_IMAGES,
     IDR_BLOCKED_IMAGES,
     gfx::VectorIconId::IMAGE,
     IDS_BLOCKED_IMAGES_TITLE,
     0,
     0,
     0},
    {CONTENT_SETTINGS_TYPE_JAVASCRIPT,
     IDR_BLOCKED_JAVASCRIPT,
     gfx::VectorIconId::CODE,
     IDS_BLOCKED_JAVASCRIPT_TITLE,
     0,
     0,
     0},
    {CONTENT_SETTINGS_TYPE_PLUGINS,
     IDR_BLOCKED_PLUGINS,
     gfx::VectorIconId::EXTENSION,
     IDS_BLOCKED_PLUGINS_MESSAGE,
     IDS_BLOCKED_PLUGIN_EXPLANATORY_TEXT,
     0,
     0},
    {CONTENT_SETTINGS_TYPE_POPUPS,
     IDR_BLOCKED_POPUPS,
     gfx::VectorIconId::WEB,
     IDS_BLOCKED_POPUPS_TOOLTIP,
     IDS_BLOCKED_POPUPS_EXPLANATORY_TEXT,
     0,
     0},
    {CONTENT_SETTINGS_TYPE_MIXEDSCRIPT,
     IDR_BLOCKED_MIXED_CONTENT,
     gfx::VectorIconId::MIXED_CONTENT,
     IDS_BLOCKED_DISPLAYING_INSECURE_CONTENT,
     0,
     0,
     0},
    {CONTENT_SETTINGS_TYPE_PPAPI_BROKER,
     IDR_BLOCKED_PPAPI_BROKER,
     gfx::VectorIconId::EXTENSION,
     IDS_BLOCKED_PPAPI_BROKER_TITLE,
     0,
     IDR_BLOCKED_PPAPI_BROKER,
     IDS_ALLOWED_PPAPI_BROKER_TITLE},
    {CONTENT_SETTINGS_TYPE_AUTOMATIC_DOWNLOADS,
     IDR_BLOCKED_DOWNLOADS,
     gfx::VectorIconId::FILE_DOWNLOAD,
     IDS_BLOCKED_DOWNLOAD_TITLE,
     IDS_BLOCKED_DOWNLOADS_EXPLANATION,
     IDR_ALLOWED_DOWNLOADS,
     IDS_ALLOWED_DOWNLOAD_TITLE},
};

const ContentSettingsImageDetails* GetImageDetails(ContentSettingsType type) {
  for (const ContentSettingsImageDetails& image_details : kImageDetails) {
    if (image_details.type == type)
      return &image_details;
  }
  return nullptr;
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

  ContentSettingsType type = get_content_settings_type();
  const ContentSettingsImageDetails* image_details = GetImageDetails(type);

  int icon_id = image_details->blocked_icon_id;
  int tooltip_id = image_details->blocked_tooltip_id;
  int explanation_id = image_details->blocked_explanatory_text_id;

  // For plugins, don't show the animated explanation unless the plugin was
  // blocked despite the user's content settings being set to allow it (e.g.
  // due to auto-blocking NPAPI plugins).
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(profile);
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

    icon_id = image_details->accessed_icon_id;
    tooltip_id = image_details->accessed_tooltip_id;
    explanation_id = 0;
  }
  set_visible(true);
  if (!UseVectorGraphics()) {
    DCHECK(icon_id);
    SetIconByResourceId(icon_id);
#if !defined(OS_MACOSX)
  } else {
    if (type == CONTENT_SETTINGS_TYPE_PPAPI_BROKER) {
      set_icon(GetIcon(image_details->vector_icon_id,
                       gfx::VectorIconId::WARNING_BADGE));
    } else {
      SetIconByVectorId(image_details->vector_icon_id,
                        content_settings->IsContentBlocked(type));
    }
#endif
  }
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
  if (!UseVectorGraphics())
    SetIconByResourceId(allowed ? IDR_ALLOWED_LOCATION : IDR_BLOCKED_LOCATION);
#if !defined(OS_MACOSX)
  else
    SetIconByVectorId(gfx::VectorIconId::MY_LOCATION, !allowed);
#endif
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
    if (!UseVectorGraphics())
      SetIconByResourceId(IDR_BLOCKED_CAMERA);
#if !defined(OS_MACOSX)
    else
      SetIconByVectorId(gfx::VectorIconId::VIDEOCAM, true);
#endif
    if (is_mic)
      id = is_cam ? IDS_MICROPHONE_CAMERA_BLOCKED : IDS_MICROPHONE_BLOCKED;
  } else {
    if (!UseVectorGraphics())
      SetIconByResourceId(IDR_ALLOWED_CAMERA);
#if !defined(OS_MACOSX)
    else
      SetIconByVectorId(gfx::VectorIconId::VIDEOCAM, false);
#endif
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
  if (!UseVectorGraphics())
    SetIconByResourceId(IDR_REGISTER_PROTOCOL_HANDLER);
#if !defined(OS_MACOSX)
  else
    SetIconByVectorId(gfx::VectorIconId::PROTOCOL_HANDLER, false);
#endif
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
#if defined(OS_MACOSX)
  SetIconByResourceId(allowed ? IDR_ALLOWED_MIDI_SYSEX
                              : IDR_BLOCKED_MIDI_SYSEX);
#else
  SetIconByVectorId(gfx::VectorIconId::MIDI, !allowed);
#endif
  set_tooltip(l10n_util::GetStringUTF8(allowed ?
      IDS_MIDI_SYSEX_ALLOWED_TOOLTIP : IDS_MIDI_SYSEX_BLOCKED_TOOLTIP));
}

ContentSettingImageModel::ContentSettingImageModel(
    ContentSettingsType content_settings_type)
    : content_settings_type_(content_settings_type),
      is_visible_(false),
      icon_id_(0),
      explanatory_string_id_(0) {
}

// static
ContentSettingImageModel*
    ContentSettingImageModel::CreateContentSettingImageModel(
    ContentSettingsType content_settings_type) {
  if (content_settings_type == CONTENT_SETTINGS_TYPE_GEOLOCATION) {
    return new ContentSettingGeolocationImageModel();
  } else if (content_settings_type == CONTENT_SETTINGS_TYPE_NOTIFICATIONS) {
    return new ContentSettingNotificationsImageModel();
  } else if (content_settings_type == CONTENT_SETTINGS_TYPE_PROTOCOL_HANDLERS) {
    return new ContentSettingRPHImageModel();
  } else if (content_settings_type == CONTENT_SETTINGS_TYPE_MEDIASTREAM) {
    return new ContentSettingMediaImageModel(content_settings_type);
  } else if (content_settings_type == CONTENT_SETTINGS_TYPE_MIDI_SYSEX) {
    return new ContentSettingMIDISysExImageModel();
  } else {
    return new ContentSettingBlockedImageModel(content_settings_type);
  }
}

void ContentSettingImageModel::SetIconByResourceId(int id) {
  icon_id_ = id;
  icon_ = ui::ResourceBundle::GetSharedInstance().GetNativeImageNamed(id);
}

#if !defined(OS_MACOSX)
void ContentSettingImageModel::SetIconByVectorId(gfx::VectorIconId id,
                                                 bool blocked) {
  icon_ = GetIcon(id, blocked ? gfx::VectorIconId::BLOCKED_BADGE
                              : gfx::VectorIconId::VECTOR_ICON_NONE);
}
#endif
