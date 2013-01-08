// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/content_settings/content_setting_bubble_model.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/api/infobars/infobar_service.h"
#include "chrome/browser/content_settings/content_settings_utils.h"
#include "chrome/browser/content_settings/cookie_settings.h"
#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry_factory.h"
#include "chrome/browser/favicon/favicon_tab_helper.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/blocked_content/blocked_content_tab_helper.h"
#include "chrome/browser/ui/blocked_content/blocked_content_tab_helper_delegate.h"
#include "chrome/browser/ui/collected_cookies_infobar_delegate.h"
#include "chrome/browser/ui/content_settings/content_setting_bubble_model_delegate.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/content_settings.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/render_messages.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "grit/generated_resources.h"
#include "net/base/net_util.h"
#include "ui/base/l10n/l10n_util.h"

using content::UserMetricsAction;
using content::WebContents;
using content_settings::SettingInfo;
using content_settings::SettingSource;
using content_settings::SETTING_SOURCE_USER;
using content_settings::SETTING_SOURCE_NONE;

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

ContentSettingTitleAndLinkModel::ContentSettingTitleAndLinkModel(
    Delegate* delegate,
    WebContents* web_contents,
    Profile* profile,
    ContentSettingsType content_type)
    : ContentSettingBubbleModel(web_contents, profile, content_type),
        delegate_(delegate) {
   // Notifications do not have a bubble.
   DCHECK_NE(content_type, CONTENT_SETTINGS_TYPE_NOTIFICATIONS);
   SetBlockedResources();
   SetTitle();
   SetManageLink();
}

void ContentSettingTitleAndLinkModel::SetBlockedResources() {
  TabSpecificContentSettings* settings =
      TabSpecificContentSettings::FromWebContents(web_contents());
  const std::set<std::string>& resources = settings->BlockedResourcesForType(
      content_type());
  for (std::set<std::string>::const_iterator it = resources.begin();
       it != resources.end(); ++it) {
    AddBlockedResource(*it);
  }
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
  };
  // Fields as for kBlockedTitleIDs, above.
  static const ContentSettingsTypeIdEntry
      kResourceSpecificBlockedTitleIDs[] = {
        {CONTENT_SETTINGS_TYPE_PLUGINS, IDS_BLOCKED_PLUGINS_TITLE},
      };
  static const ContentSettingsTypeIdEntry kAccessedTitleIDs[] = {
    {CONTENT_SETTINGS_TYPE_COOKIES, IDS_ACCESSED_COOKIES_TITLE},
  };
  const ContentSettingsTypeIdEntry *title_ids = kBlockedTitleIDs;
  size_t num_title_ids = arraysize(kBlockedTitleIDs);
  if (web_contents() &&
      TabSpecificContentSettings::FromWebContents(
          web_contents())->IsContentAccessed(content_type()) &&
      !TabSpecificContentSettings::FromWebContents(
          web_contents())->IsContentBlocked(content_type())) {
    title_ids = kAccessedTitleIDs;
    num_title_ids = arraysize(kAccessedTitleIDs);
  } else if (!bubble_content().resource_identifiers.empty()) {
    title_ids = kResourceSpecificBlockedTitleIDs;
    num_title_ids = arraysize(kResourceSpecificBlockedTitleIDs);
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
    {CONTENT_SETTINGS_TYPE_PROTOCOL_HANDLERS, IDS_HANDLERS_BUBBLE_MANAGE_LINK}
  };
  set_manage_link(l10n_util::GetStringUTF8(
      GetIdForContentType(kLinkIDs, arraysize(kLinkIDs), content_type())));
}

void ContentSettingTitleAndLinkModel::OnManageLinkClicked() {
  if (delegate_)
    delegate_->ShowContentSettingsPage(content_type());
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
  virtual void OnCustomLinkClicked() {}
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

 private:
  void SetRadioGroup();
  void AddException(ContentSetting setting,
                    const std::string& resource_identifier);
  virtual void OnRadioClicked(int radio_index);

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
        selected_item_ == 0 ? CONTENT_SETTING_ALLOW : block_setting_;
    const std::set<std::string>& resources =
        bubble_content().resource_identifiers;
    if (resources.empty()) {
      AddException(setting, std::string());
    } else {
      for (std::set<std::string>::const_iterator it = resources.begin();
           it != resources.end(); ++it) {
        AddException(setting, *it);
      }
    }
  }
}

bool ContentSettingSingleRadioGroup::settings_changed() const {
  return selected_item_ != bubble_content().radio_group.default_item;
}

// Initialize the radio group by setting the appropriate labels for the
// content type and setting the default value based on the content setting.
void ContentSettingSingleRadioGroup::SetRadioGroup() {
  GURL url = web_contents()->GetURL();
  string16 display_host_utf16;
  net::AppendFormattedHost(
      url,
      profile()->GetPrefs()->GetString(prefs::kAcceptLanguages),
      &display_host_utf16);
  std::string display_host(UTF16ToUTF8(display_host_utf16));

  if (display_host.empty())
    display_host = url.spec();

  const std::set<std::string>& resources =
      bubble_content().resource_identifiers;

  RadioGroup radio_group;
  radio_group.url = url;

  static const ContentSettingsTypeIdEntry kAllowIDs[] = {
    {CONTENT_SETTINGS_TYPE_COOKIES, IDS_BLOCKED_COOKIES_UNBLOCK},
    {CONTENT_SETTINGS_TYPE_IMAGES, IDS_BLOCKED_IMAGES_UNBLOCK},
    {CONTENT_SETTINGS_TYPE_JAVASCRIPT, IDS_BLOCKED_JAVASCRIPT_UNBLOCK},
    {CONTENT_SETTINGS_TYPE_PLUGINS, IDS_BLOCKED_PLUGINS_UNBLOCK_ALL},
    {CONTENT_SETTINGS_TYPE_POPUPS, IDS_BLOCKED_POPUPS_UNBLOCK},
  };
  // Fields as for kAllowIDs, above.
  static const ContentSettingsTypeIdEntry kResourceSpecificAllowIDs[] = {
    {CONTENT_SETTINGS_TYPE_PLUGINS, IDS_BLOCKED_PLUGINS_UNBLOCK},
  };
  std::string radio_allow_label;
  const ContentSettingsTypeIdEntry* allow_ids = resources.empty() ?
      kAllowIDs : kResourceSpecificAllowIDs;
  size_t num_allow_ids = resources.empty() ?
      arraysize(kAllowIDs) : arraysize(kResourceSpecificAllowIDs);
  radio_allow_label = l10n_util::GetStringFUTF8(
      GetIdForContentType(allow_ids, num_allow_ids, content_type()),
      UTF8ToUTF16(display_host));

  static const ContentSettingsTypeIdEntry kBlockIDs[] = {
    {CONTENT_SETTINGS_TYPE_COOKIES, IDS_BLOCKED_COOKIES_NO_ACTION},
    {CONTENT_SETTINGS_TYPE_IMAGES, IDS_BLOCKED_IMAGES_NO_ACTION},
    {CONTENT_SETTINGS_TYPE_JAVASCRIPT, IDS_BLOCKED_JAVASCRIPT_NO_ACTION},
    {CONTENT_SETTINGS_TYPE_PLUGINS, IDS_BLOCKED_PLUGINS_NO_ACTION},
    {CONTENT_SETTINGS_TYPE_POPUPS, IDS_BLOCKED_POPUPS_NO_ACTION},
  };
  std::string radio_block_label;
  radio_block_label = l10n_util::GetStringUTF8(
      GetIdForContentType(kBlockIDs, arraysize(kBlockIDs), content_type()));

  radio_group.radio_items.push_back(radio_allow_label);
  radio_group.radio_items.push_back(radio_block_label);
  HostContentSettingsMap* map = profile()->GetHostContentSettingsMap();
  CookieSettings* cookie_settings =
      CookieSettings::Factory::GetForProfile(profile());
  ContentSetting most_restrictive_setting;
  SettingSource most_restrictive_setting_source = SETTING_SOURCE_NONE;

  if (resources.empty()) {
    if (content_type() == CONTENT_SETTINGS_TYPE_COOKIES) {
      most_restrictive_setting = cookie_settings->GetCookieSetting(
          url, url, true, &most_restrictive_setting_source);
    } else {
      SettingInfo info;
      scoped_ptr<Value> value(map->GetWebsiteSetting(
          url, url, content_type(), std::string(), &info));
      most_restrictive_setting =
          content_settings::ValueToContentSetting(value.get());
      most_restrictive_setting_source = info.source;
    }
  } else {
    most_restrictive_setting = CONTENT_SETTING_ALLOW;
    for (std::set<std::string>::const_iterator it = resources.begin();
         it != resources.end(); ++it) {
      SettingInfo info;
      scoped_ptr<Value> value(map->GetWebsiteSetting(
          url, url, content_type(), *it, &info));
      ContentSetting setting =
          content_settings::ValueToContentSetting(value.get());
      if (setting == CONTENT_SETTING_BLOCK) {
        most_restrictive_setting = CONTENT_SETTING_BLOCK;
        most_restrictive_setting_source = info.source;
        break;
      }
      if (setting == CONTENT_SETTING_ASK) {
        most_restrictive_setting = CONTENT_SETTING_ASK;
        most_restrictive_setting_source = info.source;
      }
    }
  }
  if (most_restrictive_setting == CONTENT_SETTING_ALLOW) {
    radio_group.default_item = 0;
    // |block_setting_| is already set to |CONTENT_SETTING_BLOCK|.
  } else {
    radio_group.default_item = 1;
    block_setting_ = most_restrictive_setting;
  }
  if (most_restrictive_setting_source != SETTING_SOURCE_USER) {
    set_radio_group_enabled(false);
  } else {
    set_radio_group_enabled(true);
  }
  selected_item_ = radio_group.default_item;
  set_radio_group(radio_group);
}

void ContentSettingSingleRadioGroup::AddException(
    ContentSetting setting,
    const std::string& resource_identifier) {
  if (profile()) {
    profile()->GetHostContentSettingsMap()->AddExceptionForURL(
        bubble_content().radio_group.url,
        bubble_content().radio_group.url,
        content_type(),
        resource_identifier,
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
  if (settings_changed()) {
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

  virtual ~ContentSettingPluginBubbleModel() {}

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
  set_custom_link_enabled(web_contents &&
                          TabSpecificContentSettings::FromWebContents(
                              web_contents)->load_plugins_link_enabled());
}

void ContentSettingPluginBubbleModel::OnCustomLinkClicked() {
  content::RecordAction(UserMetricsAction("ClickToPlay_LoadAll_Bubble"));
  DCHECK(web_contents());
  content::RenderViewHost* host = web_contents()->GetRenderViewHost();
  // TODO(bauerb): We should send the identifiers of blocked plug-ins here.
  host->Send(new ChromeViewMsg_LoadBlockedPlugins(host->GetRoutingID(),
                                                  std::string()));
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
  virtual void OnPopupClicked(int index);
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
  std::vector<WebContents*> blocked_contents;
  BlockedContentTabHelper::FromWebContents(web_contents())->
      GetBlockedContents(&blocked_contents);
  for (std::vector<WebContents*>::const_iterator
       i = blocked_contents.begin(); i != blocked_contents.end(); ++i) {
    std::string title(UTF16ToUTF8((*i)->GetTitle()));
    // The popup may not have committed a load yet, in which case it won't
    // have a URL or title.
    if (title.empty())
      title = l10n_util::GetStringUTF8(IDS_TAB_LOADING_TITLE);
    PopupItem popup_item;
    popup_item.title = title;
    popup_item.image = FaviconTabHelper::FromWebContents(*i)->GetFavicon();
    popup_item.web_contents = *i;
    add_popup(popup_item);
  }
}

void ContentSettingPopupBubbleModel::OnPopupClicked(int index) {
  if (web_contents()) {
    BlockedContentTabHelper::FromWebContents(web_contents())->
        LaunchForContents(bubble_content().popup_items[index].web_contents);
  }
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
  const GeolocationSettingsState& settings =
      content_settings->geolocation_settings_state();
  GeolocationSettingsState::FormattedHostsPerState formatted_hosts_per_state;
  unsigned int tab_state_flags = 0;
  settings.GetDetailedInfo(&formatted_hosts_per_state, &tab_state_flags);
  // Divide the tab's current geolocation users into sets according to their
  // permission state.
  MaybeAddDomainList(formatted_hosts_per_state[CONTENT_SETTING_ALLOW],
                     IDS_GEOLOCATION_BUBBLE_SECTION_ALLOWED);

  MaybeAddDomainList(formatted_hosts_per_state[CONTENT_SETTING_BLOCK],
                     IDS_GEOLOCATION_BUBBLE_SECTION_DENIED);

  if (tab_state_flags & GeolocationSettingsState::TABSTATE_HAS_EXCEPTION) {
    set_custom_link(l10n_util::GetStringUTF8(
        IDS_GEOLOCATION_BUBBLE_CLEAR_LINK));
    set_custom_link_enabled(true);
  } else if (tab_state_flags &
             GeolocationSettingsState::TABSTATE_HAS_CHANGED) {
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
  const GeolocationSettingsState::StateMap& state_map =
      content_settings->geolocation_settings_state().state_map();
  HostContentSettingsMap* settings_map =
      profile()->GetHostContentSettingsMap();

  for (GeolocationSettingsState::StateMap::const_iterator it =
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
  content::RenderViewHost* host = web_contents()->GetRenderViewHost();
  host->Send(new ChromeViewMsg_SetAllowRunningInsecureContent(
      host->GetRoutingID(), true));
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

  string16 protocol;
  if (pending_handler_.protocol() == "mailto") {
    protocol = l10n_util::GetStringUTF16(
        IDS_REGISTER_PROTOCOL_HANDLER_MAILTO_NAME);
  } else if (pending_handler_.protocol() == "webcal") {
    protocol = l10n_util::GetStringUTF16(
        IDS_REGISTER_PROTOCOL_HANDLER_WEBCAL_NAME);
  } else {
    protocol = UTF8ToUTF16(pending_handler_.protocol());
  }

  if (previous_handler_.IsEmpty()) {
    set_title(l10n_util::GetStringFUTF8(
        IDS_REGISTER_PROTOCOL_HANDLER_CONFIRM,
        pending_handler_.title(), UTF8ToUTF16(pending_handler_.url().host()),
        protocol));
  } else {
    set_title(l10n_util::GetStringFUTF8(
        IDS_REGISTER_PROTOCOL_HANDLER_CONFIRM_REPLACE,
        pending_handler_.title(), UTF8ToUTF16(pending_handler_.url().host()),
        protocol, previous_handler_.title()));
  }

  std::string radio_allow_label =
      l10n_util::GetStringFUTF8(IDS_REGISTER_PROTOCOL_HANDLER_ACCEPT,
                                pending_handler_.title());
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
        ProtocolHandlerRegistryFactory::GetForProfile(profile);
    return new ContentSettingRPHBubbleModel(delegate, web_contents, profile,
                                            registry, content_type);
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
      content_type_(content_type) {
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

ContentSettingBubbleModel::BubbleContent::BubbleContent()
    : radio_group_enabled(false),
      custom_link_enabled(false) {
}

ContentSettingBubbleModel::BubbleContent::~BubbleContent() {}


void ContentSettingBubbleModel::AddBlockedResource(
    const std::string& resource_identifier) {
  bubble_content_.resource_identifiers.insert(resource_identifier);
}

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
