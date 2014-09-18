// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/content_settings/content_setting_bubble_model.h"

#include "base/command_line.h"
#include "base/prefs/pref_service.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/content_settings/content_settings_utils.h"
#include "chrome/browser/content_settings/cookie_settings.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry_factory.h"
#include "chrome/browser/favicon/favicon_tab_helper.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/media/media_capture_devices_dispatcher.h"
#include "chrome/browser/plugins/chrome_plugin_service_filter.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/blocked_content/popup_blocker_tab_helper.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/collected_cookies_infobar_delegate.h"
#include "chrome/browser/ui/content_settings/content_setting_bubble_model_delegate.h"
#include "chrome/browser/ui/content_settings/media_setting_changed_infobar_delegate.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/render_messages.h"
#include "chrome/grit/generated_resources.h"
#include "components/content_settings/core/common/content_settings.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "grit/components_strings.h"
#include "net/base/net_util.h"
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
  for (content::MediaStreamDevices::const_iterator it = devices.begin();
       it != devices.end(); ++it) {
    if (it->id == device_id)
      return *(it);
  }

  // A device with the |device_id| was not found. It is likely that the device
  // has been unplugged from the OS. Return the first device as the default
  // device.
  return *devices.begin();
}

}  // namespace

ContentSettingTitleAndLinkModel::ContentSettingTitleAndLinkModel(
    Delegate* delegate,
    WebContents* web_contents,
    Profile* profile,
    ContentSettingsType content_type)
    : ContentSettingBubbleModel(web_contents, profile, content_type),
        delegate_(delegate) {
  // Notifications do not have a bubble.
  DCHECK_NE(content_type, CONTENT_SETTINGS_TYPE_NOTIFICATIONS);
  SetTitle();
  SetManageLink();
  SetLearnMoreLink();
}

void ContentSettingTitleAndLinkModel::SetTitle() {
  static const ContentSettingsTypeIdEntry kBlockedTitleIDs[] = {
    {CONTENT_SETTINGS_TYPE_COOKIES, IDS_BLOCKED_COOKIES_TITLE},
    {CONTENT_SETTINGS_TYPE_IMAGES, IDS_BLOCKED_IMAGES_TITLE},
    {CONTENT_SETTINGS_TYPE_JAVASCRIPT, IDS_BLOCKED_JAVASCRIPT_TITLE},
    {CONTENT_SETTINGS_TYPE_PLUGINS, IDS_BLOCKED_PLUGINS_MESSAGE},
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
  if (web_contents() &&
      TabSpecificContentSettings::FromWebContents(
          web_contents())->IsContentAllowed(content_type()) &&
      !TabSpecificContentSettings::FromWebContents(
          web_contents())->IsContentBlocked(content_type())) {
    title_ids = kAccessedTitleIDs;
    num_title_ids = arraysize(kAccessedTitleIDs);
  }
  int title_id =
      GetIdForContentType(title_ids, num_title_ids, content_type());
  if (title_id)
    set_title(l10n_util::GetStringUTF8(title_id));
}

void ContentSettingTitleAndLinkModel::SetManageLink() {
  static const ContentSettingsTypeIdEntry kLinkIDs[] = {
    {CONTENT_SETTINGS_TYPE_COOKIES, IDS_BLOCKED_COOKIES_LINK},
    {CONTENT_SETTINGS_TYPE_IMAGES, IDS_BLOCKED_IMAGES_LINK},
    {CONTENT_SETTINGS_TYPE_JAVASCRIPT, IDS_BLOCKED_JAVASCRIPT_LINK},
    {CONTENT_SETTINGS_TYPE_PLUGINS, IDS_BLOCKED_PLUGINS_LINK},
    {CONTENT_SETTINGS_TYPE_POPUPS, IDS_BLOCKED_POPUPS_LINK},
    {CONTENT_SETTINGS_TYPE_GEOLOCATION, IDS_GEOLOCATION_BUBBLE_MANAGE_LINK},
    {CONTENT_SETTINGS_TYPE_MIXEDSCRIPT, IDS_LEARN_MORE},
    {CONTENT_SETTINGS_TYPE_PROTOCOL_HANDLERS, IDS_HANDLERS_BUBBLE_MANAGE_LINK},
    {CONTENT_SETTINGS_TYPE_MEDIASTREAM, IDS_MEDIASTREAM_BUBBLE_MANAGE_LINK},
    {CONTENT_SETTINGS_TYPE_PPAPI_BROKER, IDS_PPAPI_BROKER_BUBBLE_MANAGE_LINK},
    {CONTENT_SETTINGS_TYPE_AUTOMATIC_DOWNLOADS, IDS_BLOCKED_DOWNLOADS_LINK},
    {CONTENT_SETTINGS_TYPE_MIDI_SYSEX, IDS_MIDI_SYSEX_BUBBLE_MANAGE_LINK},
  };
  set_manage_link(l10n_util::GetStringUTF8(
      GetIdForContentType(kLinkIDs, arraysize(kLinkIDs), content_type())));
}

void ContentSettingTitleAndLinkModel::OnManageLinkClicked() {
  if (delegate_)
    delegate_->ShowContentSettingsPage(content_type());
}

void ContentSettingTitleAndLinkModel::SetLearnMoreLink() {
  static const ContentSettingsTypeIdEntry kLearnMoreIDs[] = {
    {CONTENT_SETTINGS_TYPE_PLUGINS, IDS_LEARN_MORE},
  };
  int learn_more_id =
      GetIdForContentType(kLearnMoreIDs, arraysize(kLearnMoreIDs),
                          content_type());
  if (learn_more_id)
    set_learn_more_link(l10n_util::GetStringUTF8(learn_more_id));
}

void ContentSettingTitleAndLinkModel::OnLearnMoreLinkClicked() {
  if (delegate_)
    delegate_->ShowLearnMorePage(content_type());
}

class ContentSettingTitleLinkAndCustomModel
    : public ContentSettingTitleAndLinkModel {
 public:
  ContentSettingTitleLinkAndCustomModel(Delegate* delegate,
                                        WebContents* web_contents,
                                        Profile* profile,
                                        ContentSettingsType content_type);
  virtual ~ContentSettingTitleLinkAndCustomModel() {}

 private:
  void SetCustomLink();
  virtual void OnCustomLinkClicked() OVERRIDE {}
};

ContentSettingTitleLinkAndCustomModel::ContentSettingTitleLinkAndCustomModel(
    Delegate* delegate,
    WebContents* web_contents,
    Profile* profile,
    ContentSettingsType content_type)
    : ContentSettingTitleAndLinkModel(
          delegate, web_contents, profile, content_type) {
  SetCustomLink();
}

void ContentSettingTitleLinkAndCustomModel::SetCustomLink() {
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

class ContentSettingSingleRadioGroup
    : public ContentSettingTitleLinkAndCustomModel {
 public:
  ContentSettingSingleRadioGroup(Delegate* delegate,
                                 WebContents* web_contents,
                                 Profile* profile,
                                 ContentSettingsType content_type);
  virtual ~ContentSettingSingleRadioGroup();

 protected:
  bool settings_changed() const;
  int selected_item() const { return selected_item_; }

 private:
  void SetRadioGroup();
  void AddException(ContentSetting setting);
  virtual void OnRadioClicked(int radio_index) OVERRIDE;

  ContentSetting block_setting_;
  int selected_item_;
};

ContentSettingSingleRadioGroup::ContentSettingSingleRadioGroup(
    Delegate* delegate,
    WebContents* web_contents,
    Profile* profile,
    ContentSettingsType content_type)
    : ContentSettingTitleLinkAndCustomModel(delegate, web_contents, profile,
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
    AddException(setting);
  }
}

bool ContentSettingSingleRadioGroup::settings_changed() const {
  return selected_item_ != bubble_content().radio_group.default_item;
}

// Initialize the radio group by setting the appropriate labels for the
// content type and setting the default value based on the content setting.
void ContentSettingSingleRadioGroup::SetRadioGroup() {
  GURL url = web_contents()->GetURL();
  base::string16 display_host;
  net::AppendFormattedHost(
      url,
      profile()->GetPrefs()->GetString(prefs::kAcceptLanguages),
      &display_host);

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
    // TODO(bauerb): The string shouldn't be "unblock" (they weren't blocked).
    {CONTENT_SETTINGS_TYPE_COOKIES, IDS_BLOCKED_COOKIES_UNBLOCK},
    {CONTENT_SETTINGS_TYPE_PPAPI_BROKER, IDS_ALLOWED_PPAPI_BROKER_NO_ACTION},
    {CONTENT_SETTINGS_TYPE_AUTOMATIC_DOWNLOADS, IDS_ALLOWED_DOWNLOAD_NO_ACTION},
  };

  std::string radio_allow_label;
  if (allowed) {
    int resource_id = GetIdForContentType(kAllowedAllowIDs,
                                          arraysize(kAllowedAllowIDs),
                                          content_type());
    radio_allow_label = (content_type() == CONTENT_SETTINGS_TYPE_COOKIES) ?
        l10n_util::GetStringFUTF8(resource_id, display_host) :
        l10n_util::GetStringUTF8(resource_id);
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
    // TODO(bauerb): The string should say "block".
    {CONTENT_SETTINGS_TYPE_COOKIES, IDS_BLOCKED_COOKIES_NO_ACTION},
    {CONTENT_SETTINGS_TYPE_PPAPI_BROKER, IDS_ALLOWED_PPAPI_BROKER_BLOCK},
    {CONTENT_SETTINGS_TYPE_AUTOMATIC_DOWNLOADS, IDS_ALLOWED_DOWNLOAD_BLOCK},
  };

  std::string radio_block_label;
  if (allowed) {
    int resource_id = GetIdForContentType(kAllowedBlockIDs,
                                          arraysize(kAllowedBlockIDs),
                                          content_type());
    radio_block_label = (content_type() == CONTENT_SETTINGS_TYPE_COOKIES) ?
        l10n_util::GetStringUTF8(resource_id) :
        l10n_util::GetStringFUTF8(resource_id, display_host);
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
    CookieSettings* cookie_settings =
        CookieSettings::Factory::GetForProfile(profile()).get();
    setting = cookie_settings->GetCookieSetting(
        url, url, true, &setting_source);
  } else {
    SettingInfo info;
    HostContentSettingsMap* map = profile()->GetHostContentSettingsMap();
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

  set_setting_is_managed(setting_source != SETTING_SOURCE_USER);
  if (setting_source != SETTING_SOURCE_USER) {
    set_radio_group_enabled(false);
  } else {
    set_radio_group_enabled(true);
  }
  selected_item_ = radio_group.default_item;
  set_radio_group(radio_group);
}

void ContentSettingSingleRadioGroup::AddException(ContentSetting setting) {
  if (profile()) {
    profile()->GetHostContentSettingsMap()->AddExceptionForURL(
        bubble_content().radio_group.url,
        bubble_content().radio_group.url,
        content_type(),
        setting);
  }
}

void ContentSettingSingleRadioGroup::OnRadioClicked(int radio_index) {
  selected_item_ = radio_index;
}

class ContentSettingCookiesBubbleModel : public ContentSettingSingleRadioGroup {
 public:
  ContentSettingCookiesBubbleModel(Delegate* delegate,
                                   WebContents* web_contents,
                                   Profile* profile,
                                   ContentSettingsType content_type);

  virtual ~ContentSettingCookiesBubbleModel();

 private:
  virtual void OnCustomLinkClicked() OVERRIDE;
};

ContentSettingCookiesBubbleModel::ContentSettingCookiesBubbleModel(
    Delegate* delegate,
    WebContents* web_contents,
    Profile* profile,
    ContentSettingsType content_type)
    : ContentSettingSingleRadioGroup(
          delegate, web_contents, profile, content_type) {
  DCHECK_EQ(CONTENT_SETTINGS_TYPE_COOKIES, content_type);
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

class ContentSettingPluginBubbleModel : public ContentSettingSingleRadioGroup {
 public:
  ContentSettingPluginBubbleModel(Delegate* delegate,
                                  WebContents* web_contents,
                                  Profile* profile,
                                  ContentSettingsType content_type);

  virtual ~ContentSettingPluginBubbleModel();

 private:
  virtual void OnCustomLinkClicked() OVERRIDE;
};

ContentSettingPluginBubbleModel::ContentSettingPluginBubbleModel(
    Delegate* delegate,
    WebContents* web_contents,
    Profile* profile,
    ContentSettingsType content_type)
    : ContentSettingSingleRadioGroup(
          delegate, web_contents, profile, content_type) {
  DCHECK_EQ(content_type, CONTENT_SETTINGS_TYPE_PLUGINS);
  // Disable the "Run all plugins this time" link if the setting is managed and
  // can't be controlled by the user or if the user already clicked on the link
  // and ran all plugins.
  set_custom_link_enabled(!setting_is_managed() &&
                          web_contents &&
                          TabSpecificContentSettings::FromWebContents(
                              web_contents)->load_plugins_link_enabled());
}

ContentSettingPluginBubbleModel::~ContentSettingPluginBubbleModel() {
  if (settings_changed()) {
    // If the user elected to allow all plugins then run plugins at this time.
    if (selected_item() == kAllowButtonIndex)
      OnCustomLinkClicked();
  }
}

void ContentSettingPluginBubbleModel::OnCustomLinkClicked() {
  content::RecordAction(UserMetricsAction("ClickToPlay_LoadAll_Bubble"));
  DCHECK(web_contents());
#if defined(ENABLE_PLUGINS)
  // TODO(bauerb): We should send the identifiers of blocked plug-ins here.
  ChromePluginServiceFilter::GetInstance()->AuthorizeAllPlugins(
      web_contents(), true, std::string());
#endif
  set_custom_link_enabled(false);
  TabSpecificContentSettings::FromWebContents(web_contents())->
      set_load_plugins_link_enabled(false);
}

class ContentSettingPopupBubbleModel : public ContentSettingSingleRadioGroup {
 public:
  ContentSettingPopupBubbleModel(Delegate* delegate,
                                 WebContents* web_contents,
                                 Profile* profile,
                                 ContentSettingsType content_type);
  virtual ~ContentSettingPopupBubbleModel() {}

 private:
  void SetPopups();
  virtual void OnPopupClicked(int index) OVERRIDE;
};

ContentSettingPopupBubbleModel::ContentSettingPopupBubbleModel(
    Delegate* delegate,
    WebContents* web_contents,
    Profile* profile,
    ContentSettingsType content_type)
    : ContentSettingSingleRadioGroup(
        delegate, web_contents, profile, content_type) {
  SetPopups();
}


void ContentSettingPopupBubbleModel::SetPopups() {
  std::map<int32, GURL> blocked_popups =
      PopupBlockerTabHelper::FromWebContents(web_contents())
          ->GetBlockedPopupRequests();
  for (std::map<int32, GURL>::const_iterator iter = blocked_popups.begin();
       iter != blocked_popups.end();
       ++iter) {
    std::string title(iter->second.spec());
    // The popup may not have a valid URL.
    if (title.empty())
      title = l10n_util::GetStringUTF8(IDS_TAB_LOADING_TITLE);
    PopupItem popup_item(
        ui::ResourceBundle::GetSharedInstance().GetImageNamed(
            IDR_DEFAULT_FAVICON),
        title,
        iter->first);
    add_popup(popup_item);
  }
}

void ContentSettingPopupBubbleModel::OnPopupClicked(int index) {
  if (web_contents()) {
    PopupBlockerTabHelper::FromWebContents(web_contents())->
        ShowBlockedPopup(bubble_content().popup_items[index].popup_id);
  }
}

// The model of the content settings bubble for media settings.
class ContentSettingMediaStreamBubbleModel
    : public ContentSettingTitleAndLinkModel {
 public:
  ContentSettingMediaStreamBubbleModel(Delegate* delegate,
                                       WebContents* web_contents,
                                       Profile* profile);

  virtual ~ContentSettingMediaStreamBubbleModel();

 private:
  void SetTitle();
  // Sets the data for the radio buttons of the bubble.
  void SetRadioGroup();
  // Sets the data for the media menus of the bubble.
  void SetMediaMenus();
  // Updates the camera and microphone setting with the passed |setting|.
  void UpdateSettings(ContentSetting setting);
  // Updates the camera and microphone default device with the passed |type|
  // and device.
  void UpdateDefaultDeviceForType(content::MediaStreamType type,
                                  const std::string& device);

  // ContentSettingBubbleModel implementation.
  virtual void OnRadioClicked(int radio_index) OVERRIDE;
  virtual void OnMediaMenuClicked(content::MediaStreamType type,
                                  const std::string& selected_device) OVERRIDE;

  // The index of the selected radio item.
  int selected_item_;
  // The content settings that are associated with the individual radio
  // buttons.
  ContentSetting radio_item_setting_[2];
  // The state of the microphone and camera access.
  TabSpecificContentSettings::MicrophoneCameraState state_;
};

ContentSettingMediaStreamBubbleModel::ContentSettingMediaStreamBubbleModel(
    Delegate* delegate,
    WebContents* web_contents,
    Profile* profile)
    : ContentSettingTitleAndLinkModel(
          delegate, web_contents, profile, CONTENT_SETTINGS_TYPE_MEDIASTREAM),
      selected_item_(0),
      state_(TabSpecificContentSettings::MICROPHONE_CAMERA_NOT_ACCESSED) {
  DCHECK(profile);
  // Initialize the content settings associated with the individual radio
  // buttons.
  radio_item_setting_[0] = CONTENT_SETTING_ASK;
  radio_item_setting_[1] = CONTENT_SETTING_BLOCK;

  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::FromWebContents(web_contents);
  state_ = content_settings->GetMicrophoneCameraState();

  SetTitle();
  SetRadioGroup();
  SetMediaMenus();
}

ContentSettingMediaStreamBubbleModel::~ContentSettingMediaStreamBubbleModel() {
  // On some platforms (e.g. MacOS X) it is possible to close a tab while the
  // media stream bubble is open. This resets the web contents to NULL.
  if (!web_contents())
    return;

  bool media_setting_changed = false;
  for (MediaMenuMap::const_iterator it = bubble_content().media_menus.begin();
      it != bubble_content().media_menus.end(); ++it) {
    if (it->second.selected_device.id != it->second.default_device.id) {
      UpdateDefaultDeviceForType(it->first, it->second.selected_device.id);
      media_setting_changed = true;
    }
  }

  // Update the media settings if the radio button selection was changed.
  if (selected_item_ != bubble_content().radio_group.default_item) {
    UpdateSettings(radio_item_setting_[selected_item_]);
    media_setting_changed = true;
  }

  // Trigger the reload infobar if the media setting has been changed.
  if (media_setting_changed) {
    MediaSettingChangedInfoBarDelegate::Create(
        InfoBarService::FromWebContents(web_contents()));
  }
}

void ContentSettingMediaStreamBubbleModel::SetTitle() {
  int title_id = 0;
  switch (state_) {
    case TabSpecificContentSettings::MICROPHONE_CAMERA_NOT_ACCESSED:
      // If neither microphone nor camera stream was accessed, then there is no
      // icon didplayed in the omnibox and no settings bubble availbale. Hence
      // there is no title.
      NOTREACHED();
      return;
    case TabSpecificContentSettings::MICROPHONE_ACCESSED:
      title_id = IDS_MICROPHONE_ACCESSED;
      break;
    case TabSpecificContentSettings::CAMERA_ACCESSED:
      title_id = IDS_CAMERA_ACCESSED;
      break;
    case TabSpecificContentSettings::MICROPHONE_CAMERA_ACCESSED:
      title_id = IDS_MICROPHONE_CAMERA_ALLOWED;
      break;
    case TabSpecificContentSettings::MICROPHONE_BLOCKED:
      title_id = IDS_MICROPHONE_BLOCKED;
      break;
    case TabSpecificContentSettings::CAMERA_BLOCKED:
      title_id = IDS_CAMERA_BLOCKED;
      break;
    case TabSpecificContentSettings::MICROPHONE_CAMERA_BLOCKED:
      title_id = IDS_MICROPHONE_CAMERA_BLOCKED;
      break;
  }
  set_title(l10n_util::GetStringUTF8(title_id));
}

void ContentSettingMediaStreamBubbleModel::SetRadioGroup() {
  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::FromWebContents(web_contents());
  GURL url = content_settings->media_stream_access_origin();
  RadioGroup radio_group;
  radio_group.url = url;

  base::string16 display_host_utf16;
  net::AppendFormattedHost(
      url,
      profile()->GetPrefs()->GetString(prefs::kAcceptLanguages),
      &display_host_utf16);
  std::string display_host(base::UTF16ToUTF8(display_host_utf16));
  if (display_host.empty())
    display_host = url.spec();

  int radio_allow_label_id = 0;
  int radio_block_label_id = 0;
  switch (state_) {
    case TabSpecificContentSettings::MICROPHONE_CAMERA_NOT_ACCESSED:
      NOTREACHED();
      return;
    case TabSpecificContentSettings::MICROPHONE_ACCESSED:
      radio_allow_label_id = IDS_ALLOWED_MEDIASTREAM_MIC_NO_ACTION;
      radio_block_label_id = IDS_ALLOWED_MEDIASTREAM_MIC_BLOCK;
      selected_item_ = 0;
      break;
    case TabSpecificContentSettings::CAMERA_ACCESSED:
      radio_allow_label_id = IDS_ALLOWED_MEDIASTREAM_CAMERA_NO_ACTION;
      radio_block_label_id = IDS_ALLOWED_MEDIASTREAM_CAMERA_BLOCK;
      selected_item_ = 0;
      break;
    case TabSpecificContentSettings::MICROPHONE_CAMERA_ACCESSED:
      radio_allow_label_id = IDS_ALLOWED_MEDIASTREAM_MIC_AND_CAMERA_NO_ACTION;
      radio_block_label_id = IDS_ALLOWED_MEDIASTREAM_MIC_AND_CAMERA_BLOCK;
      selected_item_ = 0;
      break;
    case TabSpecificContentSettings::MICROPHONE_BLOCKED:
      if (url.SchemeIsSecure()) {
        radio_allow_label_id = IDS_BLOCKED_MEDIASTREAM_MIC_ALLOW;
        radio_item_setting_[0] = CONTENT_SETTING_ALLOW;
      } else {
        radio_allow_label_id = IDS_BLOCKED_MEDIASTREAM_MIC_ASK;
      }

      radio_block_label_id = IDS_BLOCKED_MEDIASTREAM_MIC_NO_ACTION;
      selected_item_ = 1;
      break;
    case TabSpecificContentSettings::CAMERA_BLOCKED:
      if (url.SchemeIsSecure()) {
        radio_allow_label_id = IDS_BLOCKED_MEDIASTREAM_CAMERA_ALLOW;
        radio_item_setting_[0] = CONTENT_SETTING_ALLOW;
      } else {
        radio_allow_label_id = IDS_BLOCKED_MEDIASTREAM_CAMERA_ASK;
      }

      radio_block_label_id = IDS_BLOCKED_MEDIASTREAM_CAMERA_NO_ACTION;
      selected_item_ = 1;
      break;
    case TabSpecificContentSettings::MICROPHONE_CAMERA_BLOCKED:
      if (url.SchemeIsSecure()) {
        radio_allow_label_id = IDS_BLOCKED_MEDIASTREAM_MIC_AND_CAMERA_ALLOW;
        radio_item_setting_[0] = CONTENT_SETTING_ALLOW;
      } else {
        radio_allow_label_id = IDS_BLOCKED_MEDIASTREAM_MIC_AND_CAMERA_ASK;
      }

      radio_block_label_id = IDS_BLOCKED_MEDIASTREAM_MIC_AND_CAMERA_NO_ACTION;
      selected_item_ = 1;
      break;
  }

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
        profile()->GetHostContentSettingsMap();
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
    if (state_ == TabSpecificContentSettings::MICROPHONE_ACCESSED ||
        state_ == TabSpecificContentSettings::MICROPHONE_CAMERA_ACCESSED ||
        state_ == TabSpecificContentSettings::MICROPHONE_BLOCKED ||
        state_ == TabSpecificContentSettings::MICROPHONE_CAMERA_BLOCKED) {
      content_settings->SetContentSetting(
          primary_pattern, secondary_pattern,
          CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC, std::string(), setting);
    }
    if (state_ == TabSpecificContentSettings::CAMERA_ACCESSED ||
        state_ == TabSpecificContentSettings::MICROPHONE_CAMERA_ACCESSED ||
        state_ == TabSpecificContentSettings::CAMERA_BLOCKED ||
        state_ == TabSpecificContentSettings::MICROPHONE_CAMERA_BLOCKED) {
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

  bool show_mic_menu =
      (state_ == TabSpecificContentSettings::MICROPHONE_ACCESSED ||
       state_ == TabSpecificContentSettings::MICROPHONE_CAMERA_ACCESSED ||
       state_ == TabSpecificContentSettings::MICROPHONE_BLOCKED ||
       state_ == TabSpecificContentSettings::MICROPHONE_CAMERA_BLOCKED);
  bool show_camera_menu =
      (state_ == TabSpecificContentSettings::CAMERA_ACCESSED ||
       state_ == TabSpecificContentSettings::MICROPHONE_CAMERA_ACCESSED ||
       state_ == TabSpecificContentSettings::CAMERA_BLOCKED ||
       state_ == TabSpecificContentSettings::MICROPHONE_CAMERA_BLOCKED);
  DCHECK(show_mic_menu || show_camera_menu);

  if (show_mic_menu) {
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

  if (show_camera_menu) {
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

class ContentSettingDomainListBubbleModel
    : public ContentSettingTitleAndLinkModel {
 public:
  ContentSettingDomainListBubbleModel(Delegate* delegate,
                                      WebContents* web_contents,
                                      Profile* profile,
                                      ContentSettingsType content_type);
  virtual ~ContentSettingDomainListBubbleModel() {}

 private:
  void MaybeAddDomainList(const std::set<std::string>& hosts, int title_id);
  void SetDomainsAndCustomLink();
  virtual void OnCustomLinkClicked() OVERRIDE;
};

ContentSettingDomainListBubbleModel::ContentSettingDomainListBubbleModel(
    Delegate* delegate,
    WebContents* web_contents,
    Profile* profile,
    ContentSettingsType content_type)
    : ContentSettingTitleAndLinkModel(
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
      profile()->GetHostContentSettingsMap();

  for (ContentSettingsUsagesState::StateMap::const_iterator it =
       state_map.begin(); it != state_map.end(); ++it) {
    settings_map->SetContentSetting(
        ContentSettingsPattern::FromURLNoWildcard(it->first),
        ContentSettingsPattern::FromURLNoWildcard(embedder_url),
        CONTENT_SETTINGS_TYPE_GEOLOCATION,
        std::string(),
        CONTENT_SETTING_DEFAULT);
  }
}

class ContentSettingMixedScriptBubbleModel
    : public ContentSettingTitleLinkAndCustomModel {
 public:
  ContentSettingMixedScriptBubbleModel(Delegate* delegate,
                                       WebContents* web_contents,
                                       Profile* profile,
                                       ContentSettingsType content_type);

  virtual ~ContentSettingMixedScriptBubbleModel() {}

 private:
  virtual void OnCustomLinkClicked() OVERRIDE;
};

ContentSettingMixedScriptBubbleModel::ContentSettingMixedScriptBubbleModel(
    Delegate* delegate,
    WebContents* web_contents,
    Profile* profile,
    ContentSettingsType content_type)
    : ContentSettingTitleLinkAndCustomModel(
        delegate, web_contents, profile, content_type) {
  DCHECK_EQ(content_type, CONTENT_SETTINGS_TYPE_MIXEDSCRIPT);
  set_custom_link_enabled(true);
}

void ContentSettingMixedScriptBubbleModel::OnCustomLinkClicked() {
  content::RecordAction(UserMetricsAction("MixedScript_LoadAnyway_Bubble"));
  DCHECK(web_contents());
  web_contents()->SendToAllFrames(
      new ChromeViewMsg_SetAllowRunningInsecureContent(MSG_ROUTING_NONE, true));
  web_contents()->GetMainFrame()->Send(new ChromeViewMsg_ReloadFrame(
      web_contents()->GetMainFrame()->GetRoutingID()));
}

ContentSettingRPHBubbleModel::ContentSettingRPHBubbleModel(
    Delegate* delegate,
    WebContents* web_contents,
    Profile* profile,
    ProtocolHandlerRegistry* registry,
    ContentSettingsType content_type)
    : ContentSettingTitleAndLinkModel(
          delegate, web_contents, profile, content_type),
      selected_item_(0),
      registry_(registry),
      pending_handler_(ProtocolHandler::EmptyProtocolHandler()),
      previous_handler_(ProtocolHandler::EmptyProtocolHandler()) {
  DCHECK_EQ(CONTENT_SETTINGS_TYPE_PROTOCOL_HANDLERS, content_type);

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

class ContentSettingMidiSysExBubbleModel
    : public ContentSettingTitleAndLinkModel {
 public:
  ContentSettingMidiSysExBubbleModel(Delegate* delegate,
                                     WebContents* web_contents,
                                     Profile* profile,
                                     ContentSettingsType content_type);
  virtual ~ContentSettingMidiSysExBubbleModel() {}

 private:
  void MaybeAddDomainList(const std::set<std::string>& hosts, int title_id);
  void SetDomainsAndCustomLink();
  virtual void OnCustomLinkClicked() OVERRIDE;
};

ContentSettingMidiSysExBubbleModel::ContentSettingMidiSysExBubbleModel(
    Delegate* delegate,
    WebContents* web_contents,
    Profile* profile,
    ContentSettingsType content_type)
    : ContentSettingTitleAndLinkModel(
        delegate, web_contents, profile, content_type) {
  DCHECK_EQ(CONTENT_SETTINGS_TYPE_MIDI_SYSEX, content_type);
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
  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::FromWebContents(web_contents());
  const ContentSettingsUsagesState::StateMap& state_map =
      content_settings->midi_usages_state().state_map();
  HostContentSettingsMap* settings_map =
      profile()->GetHostContentSettingsMap();

  for (ContentSettingsUsagesState::StateMap::const_iterator it =
       state_map.begin(); it != state_map.end(); ++it) {
    settings_map->SetContentSetting(
        ContentSettingsPattern::FromURLNoWildcard(it->first),
        ContentSettingsPattern::Wildcard(),
        CONTENT_SETTINGS_TYPE_MIDI_SYSEX,
        std::string(),
        CONTENT_SETTING_DEFAULT);
  }
}

// static
ContentSettingBubbleModel*
    ContentSettingBubbleModel::CreateContentSettingBubbleModel(
        Delegate* delegate,
        WebContents* web_contents,
        Profile* profile,
        ContentSettingsType content_type) {
  if (content_type == CONTENT_SETTINGS_TYPE_COOKIES) {
    return new ContentSettingCookiesBubbleModel(delegate, web_contents, profile,
                                                content_type);
  }
  if (content_type == CONTENT_SETTINGS_TYPE_POPUPS) {
    return new ContentSettingPopupBubbleModel(delegate, web_contents, profile,
                                              content_type);
  }
  if (content_type == CONTENT_SETTINGS_TYPE_GEOLOCATION) {
    return new ContentSettingDomainListBubbleModel(delegate, web_contents,
                                                   profile, content_type);
  }
  if (content_type == CONTENT_SETTINGS_TYPE_MEDIASTREAM) {
    return new ContentSettingMediaStreamBubbleModel(delegate, web_contents,
                                                    profile);
  }
  if (content_type == CONTENT_SETTINGS_TYPE_PLUGINS) {
    return new ContentSettingPluginBubbleModel(delegate, web_contents, profile,
                                               content_type);
  }
  if (content_type == CONTENT_SETTINGS_TYPE_MIXEDSCRIPT) {
    return new ContentSettingMixedScriptBubbleModel(delegate, web_contents,
                                                    profile, content_type);
  }
  if (content_type == CONTENT_SETTINGS_TYPE_PROTOCOL_HANDLERS) {
    ProtocolHandlerRegistry* registry =
        ProtocolHandlerRegistryFactory::GetForBrowserContext(profile);
    return new ContentSettingRPHBubbleModel(delegate, web_contents, profile,
                                            registry, content_type);
  }
  if (content_type == CONTENT_SETTINGS_TYPE_MIDI_SYSEX) {
    return new ContentSettingMidiSysExBubbleModel(delegate, web_contents,
                                                  profile, content_type);
  }
  return new ContentSettingSingleRadioGroup(delegate, web_contents, profile,
                                            content_type);
}

ContentSettingBubbleModel::ContentSettingBubbleModel(
    WebContents* web_contents,
    Profile* profile,
    ContentSettingsType content_type)
    : web_contents_(web_contents),
      profile_(profile),
      content_type_(content_type),
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
