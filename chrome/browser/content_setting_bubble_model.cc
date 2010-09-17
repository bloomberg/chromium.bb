// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_setting_bubble_model.h"

#include "app/l10n_util.h"
#include "base/command_line.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/blocked_popup_container.h"
#include "chrome/browser/geolocation/geolocation_content_settings_map.h"
#include "chrome/browser/host_content_settings_map.h"
#include "chrome/browser/metrics/user_metrics.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/tab_contents/tab_contents_delegate.h"
#include "chrome/browser/tab_contents/tab_specific_content_settings.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/pref_names.h"
#include "grit/generated_resources.h"
#include "net/base/net_util.h"

class ContentSettingTitleAndLinkModel : public ContentSettingBubbleModel {
 public:
  ContentSettingTitleAndLinkModel(TabContents* tab_contents, Profile* profile,
      ContentSettingsType content_type)
      : ContentSettingBubbleModel(tab_contents, profile, content_type) {
     // Notifications do not have a bubble.
     DCHECK_NE(content_type, CONTENT_SETTINGS_TYPE_NOTIFICATIONS);
     SetBlockedResources();
     SetTitle();
     SetManageLink();
  }

 private:
  void SetBlockedResources() {
    TabSpecificContentSettings* settings =
        tab_contents()->GetTabSpecificContentSettings();
    const std::set<std::string>& resources = settings->BlockedResourcesForType(
        content_type());
    for (std::set<std::string>::const_iterator it = resources.begin();
        it != resources.end(); ++it) {
      AddBlockedResource(*it);
    }
  }

  void SetTitle() {
    static const int kBlockedTitleIDs[] = {
      IDS_BLOCKED_COOKIES_TITLE,
      IDS_BLOCKED_IMAGES_TITLE,
      IDS_BLOCKED_JAVASCRIPT_TITLE,
      IDS_BLOCKED_PLUGINS_MESSAGE,
      IDS_BLOCKED_POPUPS_TITLE,
      0,  // Geolocation does not have an overall title.
      0,  // Notifications do not have a bubble.
    };
    // Fields as for kBlockedTitleIDs, above.
    static const int kResourceSpecificBlockedTitleIDs[] = {
      0,
      0,
      0,
      IDS_BLOCKED_PLUGINS_TITLE,
      0,
      0,
      0,
    };
    static const int kAccessedTitleIDs[] = {
      IDS_ACCESSED_COOKIES_TITLE,
      0,
      0,
      0,
      0,
      0,
      0,
    };
    COMPILE_ASSERT(arraysize(kAccessedTitleIDs) == CONTENT_SETTINGS_NUM_TYPES,
                   Need_a_setting_for_every_content_settings_type);
    COMPILE_ASSERT(arraysize(kBlockedTitleIDs) == CONTENT_SETTINGS_NUM_TYPES,
                   Need_a_setting_for_every_content_settings_type);
    COMPILE_ASSERT(arraysize(kResourceSpecificBlockedTitleIDs) ==
        CONTENT_SETTINGS_NUM_TYPES,
        Need_a_setting_for_every_content_settings_type);
    const int *title_ids = kBlockedTitleIDs;
    if (tab_contents() &&
        tab_contents()->GetTabSpecificContentSettings()->IsContentAccessed(
            content_type()) &&
        !tab_contents()->GetTabSpecificContentSettings()->IsContentBlocked(
            content_type())) {
      title_ids = kAccessedTitleIDs;
    } else if (!bubble_content().resource_identifiers.empty()) {
      title_ids = kResourceSpecificBlockedTitleIDs;
    }
    if (title_ids[content_type()])
      set_title(l10n_util::GetStringUTF8(title_ids[content_type()]));
  }

  void SetManageLink() {
    static const int kLinkIDs[] = {
      IDS_BLOCKED_COOKIES_LINK,
      IDS_BLOCKED_IMAGES_LINK,
      IDS_BLOCKED_JAVASCRIPT_LINK,
      IDS_BLOCKED_PLUGINS_LINK,
      IDS_BLOCKED_POPUPS_LINK,
      IDS_GEOLOCATION_BUBBLE_MANAGE_LINK,
      0,  // Notifications do not have a bubble.
    };
    COMPILE_ASSERT(arraysize(kLinkIDs) == CONTENT_SETTINGS_NUM_TYPES,
                   Need_a_setting_for_every_content_settings_type);
    set_manage_link(l10n_util::GetStringUTF8(kLinkIDs[content_type()]));
  }

  virtual void OnManageLinkClicked() {
    if (tab_contents())
      tab_contents()->delegate()->ShowContentSettingsWindow(content_type());
  }
};

class ContentSettingTitleLinkAndInfoModel
    : public ContentSettingTitleAndLinkModel {
 public:
  ContentSettingTitleLinkAndInfoModel(TabContents* tab_contents,
                                      Profile* profile,
                                      ContentSettingsType content_type)
      : ContentSettingTitleAndLinkModel(tab_contents, profile, content_type) {
    SetInfoLink();
  }

 private:
  void SetInfoLink() {
    static const int kInfoIDs[] = {
      IDS_BLOCKED_COOKIES_INFO,
      0,  // Images do not have an info link.
      0,  // Javascript doesn't have an info link.
      0,  // Plugins do not have an info link.
      0,  // Popups do not have an info link.
      0,  // Geolocation does not have an info link.
      0,  // Notifications do not have a bubble.
    };
    COMPILE_ASSERT(arraysize(kInfoIDs) == CONTENT_SETTINGS_NUM_TYPES,
                   Need_a_setting_for_every_content_settings_type);
    if (kInfoIDs[content_type()])
      set_info_link(l10n_util::GetStringUTF8(kInfoIDs[content_type()]));
  }

  virtual void OnInfoLinkClicked() {
    DCHECK(content_type() == CONTENT_SETTINGS_TYPE_COOKIES);
    if (tab_contents()) {
      NotificationService::current()->Notify(
          NotificationType::COLLECTED_COOKIES_SHOWN,
          Source<TabSpecificContentSettings>(
              tab_contents()->GetTabSpecificContentSettings()),
          NotificationService::NoDetails());
      tab_contents()->delegate()->ShowCollectedCookiesDialog(tab_contents());
    }
  }
};


class ContentSettingSingleRadioGroup : public ContentSettingTitleAndLinkModel {
 public:
  ContentSettingSingleRadioGroup(TabContents* tab_contents, Profile* profile,
      ContentSettingsType content_type)
      : ContentSettingTitleAndLinkModel(tab_contents, profile, content_type) {
    SetRadioGroup();
  }

 private:
  void SetRadioGroup() {
    GURL url = tab_contents()->GetURL();
    std::wstring display_host_wide;
    net::AppendFormattedHost(url,
        UTF8ToWide(profile()->GetPrefs()->GetString(prefs::kAcceptLanguages)),
        &display_host_wide, NULL, NULL);
    std::string display_host(WideToUTF8(display_host_wide));

    const std::set<std::string>& resources =
        bubble_content().resource_identifiers;

    RadioGroup radio_group;
    radio_group.url = url;

    static const int kAllowIDs[] = {
      0,  // We don't manage cookies here.
      IDS_BLOCKED_IMAGES_UNBLOCK,
      IDS_BLOCKED_JAVASCRIPT_UNBLOCK,
      IDS_BLOCKED_PLUGINS_UNBLOCK_ALL,
      IDS_BLOCKED_POPUPS_UNBLOCK,
      0,  // We don't manage geolocation here.
      0,  // Notifications do not have a bubble.
    };
    COMPILE_ASSERT(arraysize(kAllowIDs) == CONTENT_SETTINGS_NUM_TYPES,
                   Need_a_setting_for_every_content_settings_type);
     // Fields as for kAllowIDs, above.
    static const int kResourceSpecificAllowIDs[] = {
      0,
      0,
      0,
      IDS_BLOCKED_PLUGINS_UNBLOCK,
      0,
      0,
      0,
    };
    COMPILE_ASSERT(
        arraysize(kResourceSpecificAllowIDs) == CONTENT_SETTINGS_NUM_TYPES,
        Need_a_setting_for_every_content_settings_type);
    std::string radio_allow_label;
    const int* allowIDs = resources.empty() ?
        kAllowIDs : kResourceSpecificAllowIDs;
    radio_allow_label = l10n_util::GetStringFUTF8(
        allowIDs[content_type()], UTF8ToUTF16(display_host));

    static const int kBlockIDs[] = {
      0,  // We don't manage cookies here.
      IDS_BLOCKED_IMAGES_NO_ACTION,
      IDS_BLOCKED_JAVASCRIPT_NO_ACTION,
      IDS_BLOCKED_PLUGINS_NO_ACTION,
      IDS_BLOCKED_POPUPS_NO_ACTION,
      0,  // We don't manage geolocation here.
      0,  // Notifications do not have a bubble.
    };
    COMPILE_ASSERT(arraysize(kBlockIDs) == CONTENT_SETTINGS_NUM_TYPES,
                   Need_a_setting_for_every_content_settings_type);
    std::string radio_block_label;
    radio_block_label = l10n_util::GetStringFUTF8(
        kBlockIDs[content_type()], UTF8ToUTF16(display_host));

    radio_group.radio_items.push_back(radio_allow_label);
    radio_group.radio_items.push_back(radio_block_label);
    HostContentSettingsMap* map = profile()->GetHostContentSettingsMap();
    if (resources.empty()) {
      ContentSetting setting = map->GetContentSetting(url, content_type(),
                                                      std::string());
      radio_group.default_item = (setting == CONTENT_SETTING_ALLOW) ? 0 : 1;
    } else {
      // The default item is "block" if at least one of the resources
      // is blocked.
      radio_group.default_item = 0;
      for (std::set<std::string>::const_iterator it = resources.begin();
           it != resources.end(); ++it) {
        ContentSetting setting = map->GetContentSetting(
            url, content_type(), *it);
        if (setting == CONTENT_SETTING_BLOCK) {
          radio_group.default_item = 1;
          break;
        }
      }
    }
    set_radio_group(radio_group);
  }

  void AddException(ContentSetting setting,
                    const std::string& resource_identifier) {
    profile()->GetHostContentSettingsMap()->AddExceptionForURL(
        bubble_content().radio_group.url, content_type(), resource_identifier,
        setting);
  }

  virtual void OnRadioClicked(int radio_index) {
    ContentSetting setting =
        radio_index == 0 ? CONTENT_SETTING_ALLOW : CONTENT_SETTING_BLOCK;
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
};

class ContentSettingPluginBubbleModel : public ContentSettingSingleRadioGroup {
 public:
  ContentSettingPluginBubbleModel(TabContents* tab_contents, Profile* profile,
                                  ContentSettingsType content_type)
      : ContentSettingSingleRadioGroup(tab_contents, profile, content_type) {
    DCHECK_EQ(content_type, CONTENT_SETTINGS_TYPE_PLUGINS);
    SetLoadPluginsLinkTitle();
  }

 private:
  void SetLoadPluginsLinkTitle() {
    if (!CommandLine::ForCurrentProcess()->HasSwitch(
        switches::kDisableClickToPlay)) {
      set_load_plugins_link_title(
          l10n_util::GetStringUTF8(IDS_BLOCKED_PLUGINS_LOAD_ALL));
     }
   }

  virtual void OnLoadPluginsLinkClicked() {
    DCHECK(!CommandLine::ForCurrentProcess()->HasSwitch(
           switches::kDisableClickToPlay));
    UserMetrics::RecordAction(UserMetricsAction("ClickToPlay_LoadAll_Bubble"));
    if (tab_contents()) {
      tab_contents()->render_view_host()->LoadBlockedPlugins();
    }
    set_load_plugins_link_enabled(false);
    TabSpecificContentSettings* settings =
        tab_contents()->GetTabSpecificContentSettings();
    settings->set_load_plugins_link_enabled(false);
  }
};

class ContentSettingPopupBubbleModel : public ContentSettingSingleRadioGroup {
 public:
  ContentSettingPopupBubbleModel(TabContents* tab_contents, Profile* profile,
      ContentSettingsType content_type)
      : ContentSettingSingleRadioGroup(tab_contents, profile, content_type) {
    SetPopups();
  }

 private:
  void SetPopups() {
    // check for crbug.com/53176
    if (!tab_contents()->blocked_popup_container())
      return;
    BlockedPopupContainer::BlockedContents blocked_contents;
    tab_contents()->blocked_popup_container()->GetBlockedContents(
        &blocked_contents);
    for (BlockedPopupContainer::BlockedContents::const_iterator
         i(blocked_contents.begin()); i != blocked_contents.end(); ++i) {
      std::string title(UTF16ToUTF8((*i)->GetTitle()));
      // The popup may not have committed a load yet, in which case it won't
      // have a URL or title.
      if (title.empty())
        title = l10n_util::GetStringUTF8(IDS_TAB_LOADING_TITLE);
      PopupItem popup_item;
      popup_item.title = title;
      popup_item.bitmap = (*i)->GetFavIcon();
      popup_item.tab_contents = (*i);
      add_popup(popup_item);
    }
  }

  virtual void OnPopupClicked(int index) {
    if (tab_contents() && tab_contents()->blocked_popup_container()) {
      tab_contents()->blocked_popup_container()->LaunchPopupForContents(
          bubble_content().popup_items[index].tab_contents);
    }
  }
};

class ContentSettingDomainListBubbleModel
    : public ContentSettingTitleAndLinkModel {
 public:
  ContentSettingDomainListBubbleModel(TabContents* tab_contents,
                                      Profile* profile,
                                      ContentSettingsType content_type)
      : ContentSettingTitleAndLinkModel(tab_contents, profile, content_type) {
    DCHECK_EQ(CONTENT_SETTINGS_TYPE_GEOLOCATION, content_type) <<
        "SetDomains currently only supports geolocation content type";
    SetDomainsAndClearLink();
  }

 private:
  void MaybeAddDomainList(const std::set<std::string>& hosts, int title_id) {
    if (!hosts.empty()) {
      DomainList domain_list;
      domain_list.title = l10n_util::GetStringUTF8(title_id);
      domain_list.hosts = hosts;
      add_domain_list(domain_list);
    }
  }
  void SetDomainsAndClearLink() {
    TabSpecificContentSettings* content_settings =
        tab_contents()->GetTabSpecificContentSettings();
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
      set_clear_link(
          l10n_util::GetStringUTF8(IDS_GEOLOCATION_BUBBLE_CLEAR_LINK));
    } else if (tab_state_flags &
               GeolocationSettingsState::TABSTATE_HAS_CHANGED) {
      // It is a slight abuse of the domain list field to use it for the reload
      // hint, but works fine for now. TODO(joth): If we need to style it
      // differently, consider adding an explicit field, or generalize the
      // domain list to be a flat list of style formatted lines.
      DomainList reload_section;
      reload_section.title = l10n_util::GetStringUTF8(
          IDS_GEOLOCATION_BUBBLE_REQUIRE_RELOAD_TO_CLEAR);
      add_domain_list(reload_section);
    }
  }
  virtual void OnClearLinkClicked() {
    if (!tab_contents())
      return;
    // Reset this embedder's entry to default for each of the requesting
    // origins currently on the page.
    const GURL& embedder_url = tab_contents()->GetURL();
    TabSpecificContentSettings* content_settings =
        tab_contents()->GetTabSpecificContentSettings();
    const GeolocationSettingsState::StateMap& state_map =
        content_settings->geolocation_settings_state().state_map();
    GeolocationContentSettingsMap* settings_map =
        profile()->GetGeolocationContentSettingsMap();
    for (GeolocationSettingsState::StateMap::const_iterator it =
         state_map.begin(); it != state_map.end(); ++it) {
      settings_map->SetContentSetting(it->first, embedder_url,
                                      CONTENT_SETTING_DEFAULT);
    }
  }
};

// static
ContentSettingBubbleModel*
    ContentSettingBubbleModel::CreateContentSettingBubbleModel(
        TabContents* tab_contents,
        Profile* profile,
        ContentSettingsType content_type) {
  if (content_type == CONTENT_SETTINGS_TYPE_COOKIES) {
    return new ContentSettingTitleLinkAndInfoModel(tab_contents, profile,
                                                   content_type);
  }
  if (content_type == CONTENT_SETTINGS_TYPE_POPUPS) {
    return new ContentSettingPopupBubbleModel(tab_contents, profile,
                                              content_type);
  }
  if (content_type == CONTENT_SETTINGS_TYPE_GEOLOCATION) {
    return new ContentSettingDomainListBubbleModel(tab_contents, profile,
                                                   content_type);
  }
  if (content_type == CONTENT_SETTINGS_TYPE_PLUGINS) {
    return new ContentSettingPluginBubbleModel(tab_contents, profile,
                                               content_type);
  }
  return new ContentSettingSingleRadioGroup(tab_contents, profile,
                                            content_type);
}

ContentSettingBubbleModel::ContentSettingBubbleModel(
    TabContents* tab_contents, Profile* profile,
    ContentSettingsType content_type)
    : tab_contents_(tab_contents), profile_(profile),
      content_type_(content_type) {
  if (tab_contents) {
    TabSpecificContentSettings* settings =
        tab_contents->GetTabSpecificContentSettings();
    set_load_plugins_link_enabled(settings->load_plugins_link_enabled());
  } else {
    set_load_plugins_link_enabled(true);
  }
  registrar_.Add(this, NotificationType::TAB_CONTENTS_DESTROYED,
                 Source<TabContents>(tab_contents));
}

ContentSettingBubbleModel::~ContentSettingBubbleModel() {
}

void ContentSettingBubbleModel::AddBlockedResource(
    const std::string& resource_identifier) {
  bubble_content_.resource_identifiers.insert(resource_identifier);
}

void ContentSettingBubbleModel::Observe(NotificationType type,
                                        const NotificationSource& source,
                                        const NotificationDetails& details) {
  DCHECK(type == NotificationType::TAB_CONTENTS_DESTROYED);
  DCHECK(source == Source<TabContents>(tab_contents_));
  tab_contents_ = NULL;
}
