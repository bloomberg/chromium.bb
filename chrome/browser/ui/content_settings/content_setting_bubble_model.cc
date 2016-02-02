// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/content_settings/content_setting_bubble_model.h"

#include <stddef.h>

#include "base/command_line.h"
#include "base/macros.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/content_settings/chrome_content_settings_utils.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry_factory.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/media/media_capture_devices_dispatcher.h"
#include "chrome/browser/plugins/chrome_plugin_service_filter.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/blocked_content/popup_blocker_tab_helper.h"
#include "chrome/browser/ui/collected_cookies_infobar_delegate.h"
#include "chrome/browser/ui/content_settings/content_setting_bubble_model_delegate.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/render_messages.h"
#include "chrome/grit/generated_resources.h"
#include "components/content_settings/content/common/content_settings_messages.h"
#include "components/content_settings/core/browser/content_settings_utils.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/prefs/pref_service.h"
#include "components/url_formatter/elide_url.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/common/origin_util.h"
#include "grit/components_strings.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/resources/grit/ui_resources.h"

using base::UserMetricsAction;
using content::WebContents;
using content_settings::SettingInfo;
using content_settings::SettingSource;
using content_settings::SETTING_SOURCE_USER;
using content_settings::SETTING_SOURCE_NONE;

namespace {

const int kAllowButtonIndex = 0;

// These states must match the order of appearance of the radio buttons
// in the XIB file for the Mac port.
enum RPHState {
  RPH_ALLOW = 0,
  RPH_BLOCK,
  RPH_IGNORE,
};

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

const content::MediaStreamDevice& GetMediaDeviceById(
    const std::string& device_id,
    const content::MediaStreamDevices& devices) {
  DCHECK(!devices.empty());
  for (const content::MediaStreamDevice& device : devices) {
    if (device.id == device_id)
      return device;
  }

  // A device with the |device_id| was not found. It is likely that the device
  // has been unplugged from the OS. Return the first device as the default
  // device.
  return *devices.begin();
}

}  // namespace

// ContentSettingSimpleBubbleModel ---------------------------------------------

ContentSettingSimpleBubbleModel::ContentSettingSimpleBubbleModel(
    Delegate* delegate,
    WebContents* web_contents,
    Profile* profile,
    ContentSettingsType content_type)
    : ContentSettingBubbleModel(delegate, web_contents, profile),
        content_type_(content_type) {
  // Notifications do not have a bubble.
  DCHECK_NE(content_type, CONTENT_SETTINGS_TYPE_NOTIFICATIONS);
  SetTitle();
  SetManageLink();
  SetCustomLink();
}

ContentSettingSimpleBubbleModel*
    ContentSettingSimpleBubbleModel::AsSimpleBubbleModel() {
  return this;
}

void ContentSettingSimpleBubbleModel::SetTitle() {
  TabSpecificContentSettings* content_settings = NULL;
  if (web_contents()) {
    content_settings =
        TabSpecificContentSettings::FromWebContents(web_contents());
  }

  static const ContentSettingsTypeIdEntry kBlockedTitleIDs[] = {
    {CONTENT_SETTINGS_TYPE_COOKIES, IDS_BLOCKED_COOKIES_TITLE},
    {CONTENT_SETTINGS_TYPE_IMAGES, IDS_BLOCKED_IMAGES_TITLE},
    {CONTENT_SETTINGS_TYPE_JAVASCRIPT, IDS_BLOCKED_JAVASCRIPT_TITLE},
    {CONTENT_SETTINGS_TYPE_PLUGINS, IDS_BLOCKED_PLUGINS_TITLE},
    {CONTENT_SETTINGS_TYPE_POPUPS, IDS_BLOCKED_POPUPS_TITLE},
    {CONTENT_SETTINGS_TYPE_MIXEDSCRIPT,
        IDS_BLOCKED_DISPLAYING_INSECURE_CONTENT},
    {CONTENT_SETTINGS_TYPE_PPAPI_BROKER,
        IDS_BLOCKED_PPAPI_BROKER_TITLE},
    {CONTENT_SETTINGS_TYPE_AUTOMATIC_DOWNLOADS, IDS_BLOCKED_DOWNLOAD_TITLE},
  };
  // Fields as for kBlockedTitleIDs, above.
  static const ContentSettingsTypeIdEntry kAccessedTitleIDs[] = {
    {CONTENT_SETTINGS_TYPE_COOKIES, IDS_ACCESSED_COOKIES_TITLE},
    {CONTENT_SETTINGS_TYPE_PPAPI_BROKER, IDS_ALLOWED_PPAPI_BROKER_TITLE},
    {CONTENT_SETTINGS_TYPE_AUTOMATIC_DOWNLOADS, IDS_ALLOWED_DOWNLOAD_TITLE},
  };
  const ContentSettingsTypeIdEntry *title_ids = kBlockedTitleIDs;
  size_t num_title_ids = arraysize(kBlockedTitleIDs);
  if (content_settings && content_settings->IsContentAllowed(content_type()) &&
      !content_settings->IsContentBlocked(content_type())) {
    title_ids = kAccessedTitleIDs;
    num_title_ids = arraysize(kAccessedTitleIDs);
  }
  int title_id =
      GetIdForContentType(title_ids, num_title_ids, content_type());
  if (title_id)
    set_title(l10n_util::GetStringUTF8(title_id));
}

void ContentSettingSimpleBubbleModel::SetManageLink() {
  static const ContentSettingsTypeIdEntry kLinkIDs[] = {
    {CONTENT_SETTINGS_TYPE_COOKIES, IDS_BLOCKED_COOKIES_LINK},
    {CONTENT_SETTINGS_TYPE_IMAGES, IDS_BLOCKED_IMAGES_LINK},
    {CONTENT_SETTINGS_TYPE_JAVASCRIPT, IDS_BLOCKED_JAVASCRIPT_LINK},
    {CONTENT_SETTINGS_TYPE_PLUGINS, IDS_BLOCKED_PLUGINS_LINK},
    {CONTENT_SETTINGS_TYPE_POPUPS, IDS_BLOCKED_POPUPS_LINK},
    {CONTENT_SETTINGS_TYPE_GEOLOCATION, IDS_GEOLOCATION_BUBBLE_MANAGE_LINK},
    {CONTENT_SETTINGS_TYPE_MIXEDSCRIPT, IDS_LEARN_MORE},
    {CONTENT_SETTINGS_TYPE_PROTOCOL_HANDLERS, IDS_HANDLERS_BUBBLE_MANAGE_LINK},
    {CONTENT_SETTINGS_TYPE_PPAPI_BROKER, IDS_PPAPI_BROKER_BUBBLE_MANAGE_LINK},
    {CONTENT_SETTINGS_TYPE_AUTOMATIC_DOWNLOADS, IDS_BLOCKED_DOWNLOADS_LINK},
    {CONTENT_SETTINGS_TYPE_MIDI_SYSEX, IDS_MIDI_SYSEX_BUBBLE_MANAGE_LINK},
  };
  set_manage_link(l10n_util::GetStringUTF8(
      GetIdForContentType(kLinkIDs, arraysize(kLinkIDs), content_type())));
}

void ContentSettingSimpleBubbleModel::OnManageLinkClicked() {
  if (delegate())
    delegate()->ShowContentSettingsPage(content_type());
}

void ContentSettingSimpleBubbleModel::SetCustomLink() {
  static const ContentSettingsTypeIdEntry kCustomIDs[] = {
    {CONTENT_SETTINGS_TYPE_COOKIES, IDS_BLOCKED_COOKIES_INFO},
    {CONTENT_SETTINGS_TYPE_PLUGINS, IDS_BLOCKED_PLUGINS_LOAD_ALL},
    {CONTENT_SETTINGS_TYPE_MIXEDSCRIPT, IDS_ALLOW_INSECURE_CONTENT_BUTTON},
  };
  int custom_link_id =
      GetIdForContentType(kCustomIDs, arraysize(kCustomIDs), content_type());
  if (custom_link_id)
    set_custom_link(l10n_util::GetStringUTF8(custom_link_id));
}

void ContentSettingSimpleBubbleModel::OnCustomLinkClicked() {
}

// ContentSettingSingleRadioGroup ----------------------------------------------

class ContentSettingSingleRadioGroup : public ContentSettingSimpleBubbleModel {
 public:
  ContentSettingSingleRadioGroup(Delegate* delegate,
                                 WebContents* web_contents,
                                 Profile* profile,
                                 ContentSettingsType content_type);
  ~ContentSettingSingleRadioGroup() override;

 protected:
  bool settings_changed() const;
  int selected_item() const { return selected_item_; }

 private:
  void SetRadioGroup();
  void SetNarrowestContentSetting(ContentSetting setting);
  void OnRadioClicked(int radio_index) override;

  ContentSetting block_setting_;
  int selected_item_;
};

ContentSettingSingleRadioGroup::ContentSettingSingleRadioGroup(
    Delegate* delegate,
    WebContents* web_contents,
    Profile* profile,
    ContentSettingsType content_type)
    : ContentSettingSimpleBubbleModel(delegate, web_contents, profile,
                                      content_type),
      block_setting_(CONTENT_SETTING_BLOCK),
      selected_item_(0) {
  SetRadioGroup();
}

ContentSettingSingleRadioGroup::~ContentSettingSingleRadioGroup() {
  if (settings_changed()) {
    ContentSetting setting =
        selected_item_ == kAllowButtonIndex ?
                          CONTENT_SETTING_ALLOW :
                          block_setting_;
    SetNarrowestContentSetting(setting);
  }
}

bool ContentSettingSingleRadioGroup::settings_changed() const {
  return selected_item_ != bubble_content().radio_group.default_item;
}

// Initialize the radio group by setting the appropriate labels for the
// content type and setting the default value based on the content setting.
void ContentSettingSingleRadioGroup::SetRadioGroup() {
  GURL url = web_contents()->GetURL();
  base::string16 display_host = url_formatter::FormatUrlForSecurityDisplay(
      url, profile()->GetPrefs()->GetString(prefs::kAcceptLanguages));
  if (display_host.empty())
    display_host = base::ASCIIToUTF16(url.spec());

  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::FromWebContents(web_contents());
  bool allowed =
      !content_settings->IsContentBlocked(content_type());
  DCHECK(!allowed ||
         content_settings->IsContentAllowed(content_type()));

  RadioGroup radio_group;
  radio_group.url = url;

  static const ContentSettingsTypeIdEntry kBlockedAllowIDs[] = {
    {CONTENT_SETTINGS_TYPE_COOKIES, IDS_BLOCKED_COOKIES_UNBLOCK},
    {CONTENT_SETTINGS_TYPE_IMAGES, IDS_BLOCKED_IMAGES_UNBLOCK},
    {CONTENT_SETTINGS_TYPE_JAVASCRIPT, IDS_BLOCKED_JAVASCRIPT_UNBLOCK},
    {CONTENT_SETTINGS_TYPE_PLUGINS, IDS_BLOCKED_PLUGINS_UNBLOCK_ALL},
    {CONTENT_SETTINGS_TYPE_POPUPS, IDS_BLOCKED_POPUPS_UNBLOCK},
    {CONTENT_SETTINGS_TYPE_PPAPI_BROKER, IDS_BLOCKED_PPAPI_BROKER_UNBLOCK},
    {CONTENT_SETTINGS_TYPE_AUTOMATIC_DOWNLOADS, IDS_BLOCKED_DOWNLOAD_UNBLOCK},
  };
  // Fields as for kBlockedAllowIDs, above.
  static const ContentSettingsTypeIdEntry kAllowedAllowIDs[] = {
    {CONTENT_SETTINGS_TYPE_COOKIES, IDS_ALLOWED_COOKIES_NO_ACTION},
    {CONTENT_SETTINGS_TYPE_PPAPI_BROKER, IDS_ALLOWED_PPAPI_BROKER_NO_ACTION},
    {CONTENT_SETTINGS_TYPE_AUTOMATIC_DOWNLOADS, IDS_ALLOWED_DOWNLOAD_NO_ACTION},
  };

  std::string radio_allow_label;
  if (allowed) {
    int resource_id = GetIdForContentType(kAllowedAllowIDs,
                                          arraysize(kAllowedAllowIDs),
                                          content_type());
    radio_allow_label = l10n_util::GetStringUTF8(resource_id);
  } else {
    radio_allow_label = l10n_util::GetStringFUTF8(
        GetIdForContentType(kBlockedAllowIDs, arraysize(kBlockedAllowIDs),
                            content_type()),
        display_host);
  }

  static const ContentSettingsTypeIdEntry kBlockedBlockIDs[] = {
    {CONTENT_SETTINGS_TYPE_COOKIES, IDS_BLOCKED_COOKIES_NO_ACTION},
    {CONTENT_SETTINGS_TYPE_IMAGES, IDS_BLOCKED_IMAGES_NO_ACTION},
    {CONTENT_SETTINGS_TYPE_JAVASCRIPT, IDS_BLOCKED_JAVASCRIPT_NO_ACTION},
    {CONTENT_SETTINGS_TYPE_PLUGINS, IDS_BLOCKED_PLUGINS_NO_ACTION},
    {CONTENT_SETTINGS_TYPE_POPUPS, IDS_BLOCKED_POPUPS_NO_ACTION},
    {CONTENT_SETTINGS_TYPE_PPAPI_BROKER, IDS_BLOCKED_PPAPI_BROKER_NO_ACTION},
    {CONTENT_SETTINGS_TYPE_AUTOMATIC_DOWNLOADS, IDS_BLOCKED_DOWNLOAD_NO_ACTION},
  };
  static const ContentSettingsTypeIdEntry kAllowedBlockIDs[] = {
    {CONTENT_SETTINGS_TYPE_COOKIES, IDS_ALLOWED_COOKIES_BLOCK},
    {CONTENT_SETTINGS_TYPE_PPAPI_BROKER, IDS_ALLOWED_PPAPI_BROKER_BLOCK},
    {CONTENT_SETTINGS_TYPE_AUTOMATIC_DOWNLOADS, IDS_ALLOWED_DOWNLOAD_BLOCK},
  };

  std::string radio_block_label;
  if (allowed) {
    int resource_id = GetIdForContentType(kAllowedBlockIDs,
                                          arraysize(kAllowedBlockIDs),
                                          content_type());
    radio_block_label = l10n_util::GetStringFUTF8(resource_id, display_host);
  } else {
    radio_block_label = l10n_util::GetStringUTF8(
        GetIdForContentType(kBlockedBlockIDs, arraysize(kBlockedBlockIDs),
                            content_type()));
  }

  radio_group.radio_items.push_back(radio_allow_label);
  radio_group.radio_items.push_back(radio_block_label);
  ContentSetting setting;
  SettingSource setting_source = SETTING_SOURCE_NONE;
  bool setting_is_wildcard = false;

  if (content_type() == CONTENT_SETTINGS_TYPE_COOKIES) {
    content_settings::CookieSettings* cookie_settings =
        CookieSettingsFactory::GetForProfile(profile()).get();
    setting = cookie_settings->GetCookieSetting(
        url, url, true, &setting_source);
  } else {
    SettingInfo info;
    HostContentSettingsMap* map =
        HostContentSettingsMapFactory::GetForProfile(profile());
    scoped_ptr<base::Value> value =
        map->GetWebsiteSetting(url, url, content_type(), std::string(), &info);
    setting = content_settings::ValueToContentSetting(value.get());
    setting_source = info.source;
    setting_is_wildcard =
        info.primary_pattern == ContentSettingsPattern::Wildcard() &&
        info.secondary_pattern == ContentSettingsPattern::Wildcard();
  }

  if (content_type() == CONTENT_SETTINGS_TYPE_PLUGINS &&
      setting == CONTENT_SETTING_ALLOW &&
      setting_is_wildcard) {
    // In the corner case of unrecognized plugins (which are now blocked by
    // default) we indicate the blocked state in the UI and allow the user to
    // whitelist.
    radio_group.default_item = 1;
  } else if (setting == CONTENT_SETTING_ALLOW) {
    radio_group.default_item = kAllowButtonIndex;
    // |block_setting_| is already set to |CONTENT_SETTING_BLOCK|.
  } else {
    radio_group.default_item = 1;
    block_setting_ = setting;
  }

  set_setting_is_managed(setting_source != SETTING_SOURCE_USER &&
                         setting != CONTENT_SETTING_ASK);
  if (setting_source != SETTING_SOURCE_USER) {
    set_radio_group_enabled(false);
  } else {
    set_radio_group_enabled(true);
  }
  selected_item_ = radio_group.default_item;
  set_radio_group(radio_group);
}

void ContentSettingSingleRadioGroup::SetNarrowestContentSetting(
    ContentSetting setting) {
  if (profile()) {
    HostContentSettingsMapFactory::GetForProfile(profile())
        ->SetNarrowestContentSetting(bubble_content().radio_group.url,
                                     bubble_content().radio_group.url,
                                     content_type(), setting);
  }
}

void ContentSettingSingleRadioGroup::OnRadioClicked(int radio_index) {
  selected_item_ = radio_index;
}

// ContentSettingCookiesBubbleModel --------------------------------------------

class ContentSettingCookiesBubbleModel : public ContentSettingSingleRadioGroup {
 public:
  ContentSettingCookiesBubbleModel(Delegate* delegate,
                                   WebContents* web_contents,
                                   Profile* profile);

  ~ContentSettingCookiesBubbleModel() override;

 private:
  void OnCustomLinkClicked() override;
};

ContentSettingCookiesBubbleModel::ContentSettingCookiesBubbleModel(
    Delegate* delegate,
    WebContents* web_contents,
    Profile* profile)
    : ContentSettingSingleRadioGroup(delegate,
                                     web_contents,
                                     profile,
                                     CONTENT_SETTINGS_TYPE_COOKIES) {
  set_custom_link_enabled(true);
}

ContentSettingCookiesBubbleModel::~ContentSettingCookiesBubbleModel() {
  // On some plattforms e.g. MacOS X it is possible to close a tab while the
  // cookies settings bubble is open. This resets the web contents to NULL.
  if (settings_changed() && web_contents()) {
    CollectedCookiesInfoBarDelegate::Create(
        InfoBarService::FromWebContents(web_contents()));
  }
}

void ContentSettingCookiesBubbleModel::OnCustomLinkClicked() {
  if (!web_contents())
    return;
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_COLLECTED_COOKIES_SHOWN,
      content::Source<TabSpecificContentSettings>(
          TabSpecificContentSettings::FromWebContents(web_contents())),
      content::NotificationService::NoDetails());
  delegate()->ShowCollectedCookiesDialog(web_contents());
}

// ContentSettingPluginBubbleModel ---------------------------------------------

class ContentSettingPluginBubbleModel : public ContentSettingSingleRadioGroup {
 public:
  ContentSettingPluginBubbleModel(Delegate* delegate,
                                  WebContents* web_contents,
                                  Profile* profile);

  ~ContentSettingPluginBubbleModel() override;

 private:
  void OnLearnMoreLinkClicked() override;
  void OnCustomLinkClicked() override;
};

ContentSettingPluginBubbleModel::ContentSettingPluginBubbleModel(
    Delegate* delegate,
    WebContents* web_contents,
    Profile* profile)
    : ContentSettingSingleRadioGroup(delegate,
                                     web_contents,
                                     profile,
                                     CONTENT_SETTINGS_TYPE_PLUGINS) {
  // Disable the "Run all plugins this time" link if the setting is managed and
  // can't be controlled by the user or if the user already clicked on the link
  // and ran all plugins.
  set_custom_link_enabled(!setting_is_managed() &&
                          web_contents &&
                          TabSpecificContentSettings::FromWebContents(
                              web_contents)->load_plugins_link_enabled());
  // Build blocked plugin list.
  if (web_contents) {
    TabSpecificContentSettings* content_settings =
        TabSpecificContentSettings::FromWebContents(web_contents);

    const std::vector<base::string16>& blocked_plugins =
        content_settings->blocked_plugin_names();
    for (const base::string16& blocked_plugin : blocked_plugins) {
      ListItem plugin_item(
          ui::ResourceBundle::GetSharedInstance().GetImageNamed(
              IDR_BLOCKED_PLUGINS),
          base::UTF16ToUTF8(blocked_plugin), false, 0);
      add_list_item(plugin_item);
    }
  }

  set_learn_more_link(l10n_util::GetStringUTF8(IDS_LEARN_MORE));
}

ContentSettingPluginBubbleModel::~ContentSettingPluginBubbleModel() {
  // If the user elected to allow all plugins then run plugins at this time.
  if (settings_changed() && selected_item() == kAllowButtonIndex)
    OnCustomLinkClicked();
}

void ContentSettingPluginBubbleModel::OnLearnMoreLinkClicked() {
  if (delegate())
    delegate()->ShowLearnMorePage(CONTENT_SETTINGS_TYPE_PLUGINS);
}

void ContentSettingPluginBubbleModel::OnCustomLinkClicked() {
  content::RecordAction(UserMetricsAction("ClickToPlay_LoadAll_Bubble"));
  // Web contents can be NULL if the tab was closed while the plugins
  // settings bubble is visible.
  if (!web_contents())
    return;
#if defined(ENABLE_PLUGINS)
  // TODO(bauerb): We should send the identifiers of blocked plugins here.
  ChromePluginServiceFilter::GetInstance()->AuthorizeAllPlugins(
      web_contents(), true, std::string());
#endif
  set_custom_link_enabled(false);
  TabSpecificContentSettings::FromWebContents(web_contents())->
      set_load_plugins_link_enabled(false);
}

// ContentSettingPopupBubbleModel ----------------------------------------------

class ContentSettingPopupBubbleModel : public ContentSettingSingleRadioGroup {
 public:
  ContentSettingPopupBubbleModel(Delegate* delegate,
                                 WebContents* web_contents,
                                 Profile* profile);
  ~ContentSettingPopupBubbleModel() override {}

 private:
  void OnListItemClicked(int index) override;

  int32_t item_id_from_item_index(int index) const {
    return bubble_content().list_items[index].item_id;
  }
};

ContentSettingPopupBubbleModel::ContentSettingPopupBubbleModel(
    Delegate* delegate,
    WebContents* web_contents,
    Profile* profile)
    : ContentSettingSingleRadioGroup(delegate,
                                     web_contents,
                                     profile,
                                     CONTENT_SETTINGS_TYPE_POPUPS) {
  if (web_contents) {
    // Build blocked popup list.
    std::map<int32_t, GURL> blocked_popups =
        PopupBlockerTabHelper::FromWebContents(web_contents)
            ->GetBlockedPopupRequests();
    for (const std::pair<int32_t, GURL>& blocked_popup : blocked_popups) {
      std::string title(blocked_popup.second.spec());
      // The pop-up may not have a valid URL.
      if (title.empty())
        title = l10n_util::GetStringUTF8(IDS_TAB_LOADING_TITLE);
      ListItem popup_item(ui::ResourceBundle::GetSharedInstance().GetImageNamed(
                              IDR_DEFAULT_FAVICON),
                          title, true, blocked_popup.first);
      add_list_item(popup_item);
    }
  }
}

void ContentSettingPopupBubbleModel::OnListItemClicked(int index) {
  if (web_contents()) {
    PopupBlockerTabHelper::FromWebContents(web_contents())
        ->ShowBlockedPopup(item_id_from_item_index(index));
  }
}

// ContentSettingMediaStreamBubbleModel ----------------------------------------

ContentSettingMediaStreamBubbleModel::ContentSettingMediaStreamBubbleModel(
    Delegate* delegate,
    WebContents* web_contents,
    Profile* profile)
    : ContentSettingBubbleModel(delegate, web_contents, profile),
      selected_item_(0),
      state_(TabSpecificContentSettings::MICROPHONE_CAMERA_NOT_ACCESSED) {
  // TODO(msramek): The media bubble has three states - mic only, camera only,
  // and both. There is a lot of duplicated code which does the same thing
  // for camera and microphone separately. Consider refactoring it to avoid
  // duplication.

  DCHECK(profile);
  // Initialize the content settings associated with the individual radio
  // buttons.
  radio_item_setting_[0] = CONTENT_SETTING_ASK;
  radio_item_setting_[1] = CONTENT_SETTING_BLOCK;

  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::FromWebContents(web_contents);
  state_ = content_settings->GetMicrophoneCameraState();
  DCHECK(CameraAccessed() || MicrophoneAccessed());

  SetTitle();
  SetRadioGroup();
  SetMediaMenus();
  SetManageLink();
  SetCustomLink();
}

ContentSettingMediaStreamBubbleModel::~ContentSettingMediaStreamBubbleModel() {
  // On some platforms (e.g. MacOS X) it is possible to close a tab while the
  // media stream bubble is open. This resets the web contents to NULL.
  if (!web_contents())
    return;

  for (const std::pair<content::MediaStreamType, MediaMenu>& media_menu :
       bubble_content().media_menus) {
    if (media_menu.second.selected_device.id !=
        media_menu.second.default_device.id) {
      UpdateDefaultDeviceForType(media_menu.first,
                                 media_menu.second.selected_device.id);
    }
  }

  // Update the media settings if the radio button selection was changed.
  if (selected_item_ != bubble_content().radio_group.default_item) {
    UpdateSettings(radio_item_setting_[selected_item_]);
  }
}

ContentSettingMediaStreamBubbleModel*
    ContentSettingMediaStreamBubbleModel::AsMediaStreamBubbleModel() {
  return this;
}

void ContentSettingMediaStreamBubbleModel::OnManageLinkClicked() {
  if (!delegate())
    return;

  if (MicrophoneAccessed() && CameraAccessed()) {
    delegate()->ShowMediaSettingsPage();
  } else {
    delegate()->ShowContentSettingsPage(CameraAccessed()
        ? CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA
        : CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC);
  }
}

bool ContentSettingMediaStreamBubbleModel::MicrophoneAccessed() const {
  return (state_ & TabSpecificContentSettings::MICROPHONE_ACCESSED) != 0;
}

bool ContentSettingMediaStreamBubbleModel::CameraAccessed() const {
  return (state_ & TabSpecificContentSettings::CAMERA_ACCESSED) != 0;
}

void ContentSettingMediaStreamBubbleModel::SetTitle() {
  DCHECK(CameraAccessed() || MicrophoneAccessed());
  int title_id = 0;
  if (state_ & TabSpecificContentSettings::MICROPHONE_BLOCKED) {
    title_id = (state_ & TabSpecificContentSettings::CAMERA_BLOCKED) ?
        IDS_MICROPHONE_CAMERA_BLOCKED : IDS_MICROPHONE_BLOCKED;
  } else if (state_ & TabSpecificContentSettings::CAMERA_BLOCKED) {
    title_id = IDS_CAMERA_BLOCKED;
  } else if (MicrophoneAccessed()) {
    title_id = CameraAccessed() ? IDS_MICROPHONE_CAMERA_ALLOWED
                                : IDS_MICROPHONE_ACCESSED;
  } else if (CameraAccessed()) {
    title_id = IDS_CAMERA_ACCESSED;
  }
  set_title(l10n_util::GetStringUTF8(title_id));
}

void ContentSettingMediaStreamBubbleModel::SetRadioGroup() {
  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::FromWebContents(web_contents());
  GURL url = content_settings->media_stream_access_origin();
  RadioGroup radio_group;
  radio_group.url = url;

  base::string16 display_host_utf16 =
      url_formatter::FormatUrlForSecurityDisplay(
          url, profile()->GetPrefs()->GetString(prefs::kAcceptLanguages));
  std::string display_host(base::UTF16ToUTF8(display_host_utf16));
  if (display_host.empty())
    display_host = url.spec();

  DCHECK(CameraAccessed() || MicrophoneAccessed());
  int radio_allow_label_id = 0;
  int radio_block_label_id = 0;
  if (state_ & (TabSpecificContentSettings::MICROPHONE_BLOCKED |
                TabSpecificContentSettings::CAMERA_BLOCKED)) {
    if (content::IsOriginSecure(url)) {
      radio_item_setting_[0] = CONTENT_SETTING_ALLOW;
      radio_allow_label_id = IDS_BLOCKED_MEDIASTREAM_CAMERA_ALLOW;
      if (MicrophoneAccessed())
        radio_allow_label_id = CameraAccessed() ?
            IDS_BLOCKED_MEDIASTREAM_MIC_AND_CAMERA_ALLOW :
            IDS_BLOCKED_MEDIASTREAM_MIC_ALLOW;
    } else {
      radio_allow_label_id = IDS_BLOCKED_MEDIASTREAM_CAMERA_ASK;
      if (MicrophoneAccessed())
        radio_allow_label_id = CameraAccessed() ?
            IDS_BLOCKED_MEDIASTREAM_MIC_AND_CAMERA_ASK :
            IDS_BLOCKED_MEDIASTREAM_MIC_ASK;
    }
    radio_block_label_id = IDS_BLOCKED_MEDIASTREAM_CAMERA_NO_ACTION;
    if (MicrophoneAccessed())
      radio_block_label_id = CameraAccessed() ?
          IDS_BLOCKED_MEDIASTREAM_MIC_AND_CAMERA_NO_ACTION :
          IDS_BLOCKED_MEDIASTREAM_MIC_NO_ACTION;
  } else {
    if (MicrophoneAccessed() && CameraAccessed()) {
      radio_allow_label_id = IDS_ALLOWED_MEDIASTREAM_MIC_AND_CAMERA_NO_ACTION;
      radio_block_label_id = IDS_ALLOWED_MEDIASTREAM_MIC_AND_CAMERA_BLOCK;
    } else if (MicrophoneAccessed()) {
      radio_allow_label_id = IDS_ALLOWED_MEDIASTREAM_MIC_NO_ACTION;
      radio_block_label_id = IDS_ALLOWED_MEDIASTREAM_MIC_BLOCK;
    } else {
      radio_allow_label_id = IDS_ALLOWED_MEDIASTREAM_CAMERA_NO_ACTION;
      radio_block_label_id = IDS_ALLOWED_MEDIASTREAM_CAMERA_BLOCK;
    }
  }
  selected_item_ =
      (MicrophoneAccessed() && content_settings->IsContentBlocked(
          CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC)) ||
      (CameraAccessed() && content_settings->IsContentBlocked(
          CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA)) ? 1 : 0;

  std::string radio_allow_label = l10n_util::GetStringFUTF8(
      radio_allow_label_id, base::UTF8ToUTF16(display_host));
  std::string radio_block_label =
      l10n_util::GetStringUTF8(radio_block_label_id);

  radio_group.default_item = selected_item_;
  radio_group.radio_items.push_back(radio_allow_label);
  radio_group.radio_items.push_back(radio_block_label);

  set_radio_group(radio_group);
  set_radio_group_enabled(true);
}

void ContentSettingMediaStreamBubbleModel::UpdateSettings(
    ContentSetting setting) {
  if (profile()) {
    HostContentSettingsMap* content_settings =
        HostContentSettingsMapFactory::GetForProfile(profile());
    TabSpecificContentSettings* tab_content_settings =
        TabSpecificContentSettings::FromWebContents(web_contents());
    // The same patterns must be used as in other places (e.g. the infobar) in
    // order to override the existing rule. Otherwise a new rule is created.
    // TODO(markusheintz): Extract to a helper so that there is only a single
    // place to touch.
    ContentSettingsPattern primary_pattern =
        ContentSettingsPattern::FromURLNoWildcard(
            tab_content_settings->media_stream_access_origin());
    ContentSettingsPattern secondary_pattern =
        ContentSettingsPattern::Wildcard();
    if (MicrophoneAccessed()) {
      content_settings->SetContentSetting(
          primary_pattern, secondary_pattern,
          CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC, std::string(), setting);
    }
    if (CameraAccessed()) {
      content_settings->SetContentSetting(
          primary_pattern, secondary_pattern,
          CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA, std::string(), setting);
    }
  }
}

void ContentSettingMediaStreamBubbleModel::UpdateDefaultDeviceForType(
    content::MediaStreamType type,
    const std::string& device) {
  PrefService* prefs = profile()->GetPrefs();
  if (type == content::MEDIA_DEVICE_AUDIO_CAPTURE) {
    prefs->SetString(prefs::kDefaultAudioCaptureDevice, device);
  } else {
    DCHECK_EQ(content::MEDIA_DEVICE_VIDEO_CAPTURE, type);
    prefs->SetString(prefs::kDefaultVideoCaptureDevice, device);
  }
}

void ContentSettingMediaStreamBubbleModel::SetMediaMenus() {
  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::FromWebContents(web_contents());
  const std::string& requested_microphone =
       content_settings->media_stream_requested_audio_device();
  const std::string& requested_camera =
      content_settings->media_stream_requested_video_device();

  // Add microphone menu.
  PrefService* prefs = profile()->GetPrefs();
  MediaCaptureDevicesDispatcher* dispatcher =
      MediaCaptureDevicesDispatcher::GetInstance();
  const content::MediaStreamDevices& microphones =
      dispatcher->GetAudioCaptureDevices();

  if (MicrophoneAccessed()) {
    MediaMenu mic_menu;
    mic_menu.label = l10n_util::GetStringUTF8(IDS_MEDIA_SELECTED_MIC_LABEL);
    if (!microphones.empty()) {
      std::string preferred_mic;
      if (requested_microphone.empty()) {
        preferred_mic = prefs->GetString(prefs::kDefaultAudioCaptureDevice);
        mic_menu.disabled = false;
      } else {
        // Set the |disabled| to true in order to disable the device selection
        // menu on the media settings bubble. This must be done if the website
        // manages the microphone devices itself.
        preferred_mic = requested_microphone;
        mic_menu.disabled = true;
      }

      mic_menu.default_device = GetMediaDeviceById(preferred_mic, microphones);
      mic_menu.selected_device = mic_menu.default_device;
    }
    add_media_menu(content::MEDIA_DEVICE_AUDIO_CAPTURE, mic_menu);
  }

  if (CameraAccessed()) {
    const content::MediaStreamDevices& cameras =
        dispatcher->GetVideoCaptureDevices();
    MediaMenu camera_menu;
    camera_menu.label =
        l10n_util::GetStringUTF8(IDS_MEDIA_SELECTED_CAMERA_LABEL);
    if (!cameras.empty()) {
      std::string preferred_camera;
      if (requested_camera.empty()) {
        preferred_camera = prefs->GetString(prefs::kDefaultVideoCaptureDevice);
        camera_menu.disabled = false;
      } else {
        // Disable the menu since the website is managing the camera devices
        // itself.
        preferred_camera = requested_camera;
        camera_menu.disabled = true;
      }

      camera_menu.default_device =
          GetMediaDeviceById(preferred_camera, cameras);
      camera_menu.selected_device = camera_menu.default_device;
    }
    add_media_menu(content::MEDIA_DEVICE_VIDEO_CAPTURE, camera_menu);
  }
}

void ContentSettingMediaStreamBubbleModel::SetManageLink() {
  // By default, the manage link refers to both media types. We only need
  // to change the link text if only one media type was accessed.
  int link_id;
  if (CameraAccessed() && MicrophoneAccessed()) {
    link_id = IDS_MEDIASTREAM_BUBBLE_MANAGE_LINK;
  } else if (CameraAccessed()) {
    link_id = IDS_MEDIASTREAM_CAMERA_BUBBLE_MANAGE_LINK;
  } else if (MicrophoneAccessed()) {
    link_id = IDS_MEDIASTREAM_MICROPHONE_BUBBLE_MANAGE_LINK;
  } else {
    NOTREACHED();
    return;
  }

  set_manage_link(l10n_util::GetStringUTF8(link_id));
}

void ContentSettingMediaStreamBubbleModel::SetCustomLink() {
  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::FromWebContents(web_contents());
  if (content_settings->IsMicrophoneCameraStateChanged()) {
    set_custom_link(l10n_util::GetStringUTF8(
        IDS_MEDIASTREAM_SETTING_CHANGED_MESSAGE));
  }
}

void ContentSettingMediaStreamBubbleModel::OnRadioClicked(int radio_index) {
  selected_item_ = radio_index;
}

void ContentSettingMediaStreamBubbleModel::OnMediaMenuClicked(
    content::MediaStreamType type,
    const std::string& selected_device_id) {
  DCHECK(type == content::MEDIA_DEVICE_AUDIO_CAPTURE ||
         type == content::MEDIA_DEVICE_VIDEO_CAPTURE);
  DCHECK_EQ(1U, bubble_content().media_menus.count(type));
  MediaCaptureDevicesDispatcher* dispatcher =
      MediaCaptureDevicesDispatcher::GetInstance();
  const content::MediaStreamDevices& devices =
      (type == content::MEDIA_DEVICE_AUDIO_CAPTURE) ?
          dispatcher->GetAudioCaptureDevices() :
          dispatcher->GetVideoCaptureDevices();
  set_selected_device(GetMediaDeviceById(selected_device_id, devices));
}

// ContentSettingDomainListBubbleModel -----------------------------------------

class ContentSettingDomainListBubbleModel
    : public ContentSettingSimpleBubbleModel {
 public:
  ContentSettingDomainListBubbleModel(Delegate* delegate,
                                      WebContents* web_contents,
                                      Profile* profile,
                                      ContentSettingsType content_type);
  ~ContentSettingDomainListBubbleModel() override {}

 private:
  void MaybeAddDomainList(const std::set<std::string>& hosts, int title_id);
  void SetDomainsAndCustomLink();
  void OnCustomLinkClicked() override;
};

ContentSettingDomainListBubbleModel::ContentSettingDomainListBubbleModel(
    Delegate* delegate,
    WebContents* web_contents,
    Profile* profile,
    ContentSettingsType content_type)
    : ContentSettingSimpleBubbleModel(
        delegate, web_contents, profile, content_type) {
  DCHECK_EQ(CONTENT_SETTINGS_TYPE_GEOLOCATION, content_type) <<
      "SetDomains currently only supports geolocation content type";
  SetDomainsAndCustomLink();
}

void ContentSettingDomainListBubbleModel::MaybeAddDomainList(
    const std::set<std::string>& hosts, int title_id) {
  if (!hosts.empty()) {
    DomainList domain_list;
    domain_list.title = l10n_util::GetStringUTF8(title_id);
    domain_list.hosts = hosts;
    add_domain_list(domain_list);
  }
}

void ContentSettingDomainListBubbleModel::SetDomainsAndCustomLink() {
  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::FromWebContents(web_contents());
  const ContentSettingsUsagesState& usages =
      content_settings->geolocation_usages_state();
  ContentSettingsUsagesState::FormattedHostsPerState formatted_hosts_per_state;
  unsigned int tab_state_flags = 0;
  usages.GetDetailedInfo(&formatted_hosts_per_state, &tab_state_flags);
  // Divide the tab's current geolocation users into sets according to their
  // permission state.
  MaybeAddDomainList(formatted_hosts_per_state[CONTENT_SETTING_ALLOW],
                     IDS_GEOLOCATION_BUBBLE_SECTION_ALLOWED);

  MaybeAddDomainList(formatted_hosts_per_state[CONTENT_SETTING_BLOCK],
                     IDS_GEOLOCATION_BUBBLE_SECTION_DENIED);

  if (tab_state_flags & ContentSettingsUsagesState::TABSTATE_HAS_EXCEPTION) {
    set_custom_link(l10n_util::GetStringUTF8(
        IDS_GEOLOCATION_BUBBLE_CLEAR_LINK));
    set_custom_link_enabled(true);
  } else if (tab_state_flags &
             ContentSettingsUsagesState::TABSTATE_HAS_CHANGED) {
    set_custom_link(l10n_util::GetStringUTF8(
        IDS_GEOLOCATION_BUBBLE_REQUIRE_RELOAD_TO_CLEAR));
  }
}

void ContentSettingDomainListBubbleModel::OnCustomLinkClicked() {
  if (!web_contents())
    return;
  // Reset this embedder's entry to default for each of the requesting
  // origins currently on the page.
  const GURL& embedder_url = web_contents()->GetURL();
  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::FromWebContents(web_contents());
  const ContentSettingsUsagesState::StateMap& state_map =
      content_settings->geolocation_usages_state().state_map();
  HostContentSettingsMap* settings_map =
      HostContentSettingsMapFactory::GetForProfile(profile());

  for (const std::pair<GURL, ContentSetting>& map_entry : state_map) {
    settings_map->SetContentSetting(
        ContentSettingsPattern::FromURLNoWildcard(map_entry.first),
        ContentSettingsPattern::FromURLNoWildcard(embedder_url),
        CONTENT_SETTINGS_TYPE_GEOLOCATION, std::string(),
        CONTENT_SETTING_DEFAULT);
  }
}

// ContentSettingMixedScriptBubbleModel ----------------------------------------

class ContentSettingMixedScriptBubbleModel
    : public ContentSettingSimpleBubbleModel {
 public:
  ContentSettingMixedScriptBubbleModel(Delegate* delegate,
                                       WebContents* web_contents,
                                       Profile* profile);

  ~ContentSettingMixedScriptBubbleModel() override {}

 private:
  void OnCustomLinkClicked() override;
};

ContentSettingMixedScriptBubbleModel::ContentSettingMixedScriptBubbleModel(
    Delegate* delegate,
    WebContents* web_contents,
    Profile* profile)
    : ContentSettingSimpleBubbleModel(
          delegate,
          web_contents,
          profile,
          CONTENT_SETTINGS_TYPE_MIXEDSCRIPT) {
  content_settings::RecordMixedScriptAction(
      content_settings::MIXED_SCRIPT_ACTION_DISPLAYED_BUBBLE);
  set_custom_link_enabled(true);
}

void ContentSettingMixedScriptBubbleModel::OnCustomLinkClicked() {
  DCHECK(web_contents());
  web_contents()->SendToAllFrames(
      new ChromeViewMsg_SetAllowRunningInsecureContent(MSG_ROUTING_NONE, true));
  web_contents()->GetMainFrame()->Send(new ChromeViewMsg_ReloadFrame(
      web_contents()->GetMainFrame()->GetRoutingID()));

  content_settings::RecordMixedScriptAction(
      content_settings::MIXED_SCRIPT_ACTION_CLICKED_ALLOW);
  content_settings::RecordMixedScriptActionWithRAPPOR(
      content_settings::MIXED_SCRIPT_ACTION_CLICKED_ALLOW,
      web_contents()->GetLastCommittedURL());
}

// ContentSettingRPHBubbleModel ------------------------------------------------

ContentSettingRPHBubbleModel::ContentSettingRPHBubbleModel(
    Delegate* delegate,
    WebContents* web_contents,
    Profile* profile,
    ProtocolHandlerRegistry* registry)
    : ContentSettingSimpleBubbleModel(delegate,
                                      web_contents,
                                      profile,
                                      CONTENT_SETTINGS_TYPE_PROTOCOL_HANDLERS),
      selected_item_(0),
      registry_(registry),
      pending_handler_(ProtocolHandler::EmptyProtocolHandler()),
      previous_handler_(ProtocolHandler::EmptyProtocolHandler()) {
  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::FromWebContents(web_contents);
  pending_handler_ = content_settings->pending_protocol_handler();
  previous_handler_ = content_settings->previous_protocol_handler();

  base::string16 protocol;
  if (pending_handler_.protocol() == "mailto") {
    protocol = l10n_util::GetStringUTF16(
        IDS_REGISTER_PROTOCOL_HANDLER_MAILTO_NAME);
  } else if (pending_handler_.protocol() == "webcal") {
    protocol = l10n_util::GetStringUTF16(
        IDS_REGISTER_PROTOCOL_HANDLER_WEBCAL_NAME);
  } else {
    protocol = base::UTF8ToUTF16(pending_handler_.protocol());
  }

  // Note that we ignore the |title| parameter.
  if (previous_handler_.IsEmpty()) {
    set_title(l10n_util::GetStringFUTF8(
        IDS_REGISTER_PROTOCOL_HANDLER_CONFIRM,
        base::UTF8ToUTF16(pending_handler_.url().host()),
        protocol));
  } else {
    set_title(l10n_util::GetStringFUTF8(
        IDS_REGISTER_PROTOCOL_HANDLER_CONFIRM_REPLACE,
        base::UTF8ToUTF16(pending_handler_.url().host()),
        protocol,
        base::UTF8ToUTF16(previous_handler_.url().host())));
  }

  std::string radio_allow_label =
      l10n_util::GetStringUTF8(IDS_REGISTER_PROTOCOL_HANDLER_ACCEPT);
  std::string radio_deny_label =
      l10n_util::GetStringUTF8(IDS_REGISTER_PROTOCOL_HANDLER_DENY);
  std::string radio_ignore_label =
      l10n_util::GetStringUTF8(IDS_REGISTER_PROTOCOL_HANDLER_IGNORE);

  GURL url = web_contents->GetURL();
  RadioGroup radio_group;
  radio_group.url = url;

  radio_group.radio_items.push_back(radio_allow_label);
  radio_group.radio_items.push_back(radio_deny_label);
  radio_group.radio_items.push_back(radio_ignore_label);
  ContentSetting setting =
      content_settings->pending_protocol_handler_setting();
  if (setting == CONTENT_SETTING_ALLOW)
    radio_group.default_item = RPH_ALLOW;
  else if (setting == CONTENT_SETTING_BLOCK)
    radio_group.default_item = RPH_BLOCK;
  else
    radio_group.default_item = RPH_IGNORE;

  selected_item_ = radio_group.default_item;
  set_radio_group_enabled(true);
  set_radio_group(radio_group);
}

void ContentSettingRPHBubbleModel::OnRadioClicked(int radio_index) {
  if (selected_item_ == radio_index)
    return;

  selected_item_ = radio_index;

  if (radio_index == RPH_ALLOW)
    RegisterProtocolHandler();
  else if (radio_index == RPH_BLOCK)
    UnregisterProtocolHandler();
  else if (radio_index == RPH_IGNORE)
    IgnoreProtocolHandler();
  else
    NOTREACHED();
}

void ContentSettingRPHBubbleModel::OnDoneClicked() {
  // The user has one chance to deal with the RPH content setting UI,
  // then we remove it.
  TabSpecificContentSettings::FromWebContents(web_contents())->
      ClearPendingProtocolHandler();
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_WEB_CONTENT_SETTINGS_CHANGED,
      content::Source<WebContents>(web_contents()),
      content::NotificationService::NoDetails());
}

void ContentSettingRPHBubbleModel::RegisterProtocolHandler() {
  // A no-op if the handler hasn't been ignored, but needed in case the user
  // selects sequences like register/ignore/register.
  registry_->RemoveIgnoredHandler(pending_handler_);

  registry_->OnAcceptRegisterProtocolHandler(pending_handler_);
  TabSpecificContentSettings::FromWebContents(web_contents())->
      set_pending_protocol_handler_setting(CONTENT_SETTING_ALLOW);
}

void ContentSettingRPHBubbleModel::UnregisterProtocolHandler() {
  registry_->OnDenyRegisterProtocolHandler(pending_handler_);
  TabSpecificContentSettings::FromWebContents(web_contents())->
      set_pending_protocol_handler_setting(CONTENT_SETTING_BLOCK);
  ClearOrSetPreviousHandler();
}

void ContentSettingRPHBubbleModel::IgnoreProtocolHandler() {
  registry_->OnIgnoreRegisterProtocolHandler(pending_handler_);
  TabSpecificContentSettings::FromWebContents(web_contents())->
      set_pending_protocol_handler_setting(CONTENT_SETTING_DEFAULT);
  ClearOrSetPreviousHandler();
}

void ContentSettingRPHBubbleModel::ClearOrSetPreviousHandler() {
  if (previous_handler_.IsEmpty()) {
    registry_->ClearDefault(pending_handler_.protocol());
  } else {
    registry_->OnAcceptRegisterProtocolHandler(previous_handler_);
  }
}

// ContentSettingMidiSysExBubbleModel ------------------------------------------

class ContentSettingMidiSysExBubbleModel
    : public ContentSettingSimpleBubbleModel {
 public:
  ContentSettingMidiSysExBubbleModel(Delegate* delegate,
                                     WebContents* web_contents,
                                     Profile* profile);
  ~ContentSettingMidiSysExBubbleModel() override {}

 private:
  void MaybeAddDomainList(const std::set<std::string>& hosts, int title_id);
  void SetDomainsAndCustomLink();
  void OnCustomLinkClicked() override;
};

ContentSettingMidiSysExBubbleModel::ContentSettingMidiSysExBubbleModel(
    Delegate* delegate,
    WebContents* web_contents,
    Profile* profile)
    : ContentSettingSimpleBubbleModel(delegate,
                                      web_contents,
                                      profile,
                                      CONTENT_SETTINGS_TYPE_MIDI_SYSEX) {
  SetDomainsAndCustomLink();
}

void ContentSettingMidiSysExBubbleModel::MaybeAddDomainList(
    const std::set<std::string>& hosts, int title_id) {
  if (!hosts.empty()) {
    DomainList domain_list;
    domain_list.title = l10n_util::GetStringUTF8(title_id);
    domain_list.hosts = hosts;
    add_domain_list(domain_list);
  }
}

void ContentSettingMidiSysExBubbleModel::SetDomainsAndCustomLink() {
  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::FromWebContents(web_contents());
  const ContentSettingsUsagesState& usages_state =
      content_settings->midi_usages_state();
  ContentSettingsUsagesState::FormattedHostsPerState formatted_hosts_per_state;
  unsigned int tab_state_flags = 0;
  usages_state.GetDetailedInfo(&formatted_hosts_per_state, &tab_state_flags);
  // Divide the tab's current MIDI sysex users into sets according to their
  // permission state.
  MaybeAddDomainList(formatted_hosts_per_state[CONTENT_SETTING_ALLOW],
                     IDS_MIDI_SYSEX_BUBBLE_ALLOWED);

  MaybeAddDomainList(formatted_hosts_per_state[CONTENT_SETTING_BLOCK],
                     IDS_MIDI_SYSEX_BUBBLE_DENIED);

  if (tab_state_flags & ContentSettingsUsagesState::TABSTATE_HAS_EXCEPTION) {
    set_custom_link(l10n_util::GetStringUTF8(
        IDS_MIDI_SYSEX_BUBBLE_CLEAR_LINK));
    set_custom_link_enabled(true);
  } else if (tab_state_flags &
             ContentSettingsUsagesState::TABSTATE_HAS_CHANGED) {
    set_custom_link(l10n_util::GetStringUTF8(
        IDS_MIDI_SYSEX_BUBBLE_REQUIRE_RELOAD_TO_CLEAR));
  }
}

void ContentSettingMidiSysExBubbleModel::OnCustomLinkClicked() {
  if (!web_contents())
    return;
  // Reset this embedder's entry to default for each of the requesting
  // origins currently on the page.
  const GURL& embedder_url = web_contents()->GetURL();
  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::FromWebContents(web_contents());
  const ContentSettingsUsagesState::StateMap& state_map =
      content_settings->midi_usages_state().state_map();
  HostContentSettingsMap* settings_map =
      HostContentSettingsMapFactory::GetForProfile(profile());

  for (const std::pair<GURL, ContentSetting>& map_entry : state_map) {
    settings_map->SetContentSetting(
        ContentSettingsPattern::FromURLNoWildcard(map_entry.first),
        ContentSettingsPattern::FromURLNoWildcard(embedder_url),
        CONTENT_SETTINGS_TYPE_MIDI_SYSEX, std::string(),
        CONTENT_SETTING_DEFAULT);
  }
}

// ContentSettingBubbleModel ---------------------------------------------------

// static
ContentSettingBubbleModel*
    ContentSettingBubbleModel::CreateContentSettingBubbleModel(
        Delegate* delegate,
        WebContents* web_contents,
        Profile* profile,
        ContentSettingsType content_type) {
  if (content_type == CONTENT_SETTINGS_TYPE_COOKIES) {
    return new ContentSettingCookiesBubbleModel(delegate, web_contents,
                                                profile);
  }
  if (content_type == CONTENT_SETTINGS_TYPE_POPUPS) {
    return new ContentSettingPopupBubbleModel(delegate, web_contents, profile);
  }
  if (content_type == CONTENT_SETTINGS_TYPE_GEOLOCATION) {
    return new ContentSettingDomainListBubbleModel(delegate, web_contents,
                                                   profile, content_type);
  }
  if (content_type == CONTENT_SETTINGS_TYPE_PLUGINS) {
    return new ContentSettingPluginBubbleModel(delegate, web_contents, profile);
  }
  if (content_type == CONTENT_SETTINGS_TYPE_MIXEDSCRIPT) {
    return new ContentSettingMixedScriptBubbleModel(delegate, web_contents,
                                                    profile);
  }
  if (content_type == CONTENT_SETTINGS_TYPE_PROTOCOL_HANDLERS) {
    ProtocolHandlerRegistry* registry =
        ProtocolHandlerRegistryFactory::GetForBrowserContext(profile);
    return new ContentSettingRPHBubbleModel(delegate, web_contents, profile,
                                            registry);
  }
  if (content_type == CONTENT_SETTINGS_TYPE_MIDI_SYSEX) {
    return new ContentSettingMidiSysExBubbleModel(delegate, web_contents,
                                                  profile);
  }
  if (content_type == CONTENT_SETTINGS_TYPE_IMAGES ||
      content_type == CONTENT_SETTINGS_TYPE_JAVASCRIPT ||
      content_type == CONTENT_SETTINGS_TYPE_PPAPI_BROKER ||
      content_type == CONTENT_SETTINGS_TYPE_AUTOMATIC_DOWNLOADS) {
    return new ContentSettingSingleRadioGroup(delegate, web_contents, profile,
                                              content_type);
  }
  NOTREACHED() << "No bubble for the content type " << content_type << ".";
  return nullptr;
}

ContentSettingBubbleModel::ContentSettingBubbleModel(
    Delegate* delegate,
    WebContents* web_contents,
    Profile* profile)
    : web_contents_(web_contents),
      profile_(profile),
      delegate_(delegate),
      setting_is_managed_(false) {
  registrar_.Add(this, content::NOTIFICATION_WEB_CONTENTS_DESTROYED,
                 content::Source<WebContents>(web_contents));
  registrar_.Add(this, chrome::NOTIFICATION_PROFILE_DESTROYED,
                 content::Source<Profile>(profile_));
}

ContentSettingBubbleModel::~ContentSettingBubbleModel() {
}

ContentSettingBubbleModel::RadioGroup::RadioGroup() : default_item(0) {}

ContentSettingBubbleModel::RadioGroup::~RadioGroup() {}

ContentSettingBubbleModel::DomainList::DomainList() {}

ContentSettingBubbleModel::DomainList::~DomainList() {}

ContentSettingBubbleModel::MediaMenu::MediaMenu() : disabled(false) {}

ContentSettingBubbleModel::MediaMenu::~MediaMenu() {}

ContentSettingBubbleModel::BubbleContent::BubbleContent()
    : radio_group_enabled(false),
      custom_link_enabled(false) {
}

ContentSettingBubbleModel::BubbleContent::~BubbleContent() {}

void ContentSettingBubbleModel::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  if (type == content::NOTIFICATION_WEB_CONTENTS_DESTROYED) {
    DCHECK_EQ(web_contents_,
              content::Source<WebContents>(source).ptr());
    web_contents_ = NULL;
  } else {
    DCHECK_EQ(chrome::NOTIFICATION_PROFILE_DESTROYED, type);
    DCHECK_EQ(profile_, content::Source<Profile>(source).ptr());
    profile_ = NULL;
  }
}

ContentSettingSimpleBubbleModel*
    ContentSettingBubbleModel::AsSimpleBubbleModel() {
  // In general, bubble models might not inherit from the simple bubble model.
  return nullptr;
}

ContentSettingMediaStreamBubbleModel*
    ContentSettingBubbleModel::AsMediaStreamBubbleModel() {
  // In general, bubble models might not inherit from the media bubble model.
  return nullptr;
}
