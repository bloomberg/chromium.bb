// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_setting_bubble_model.h"

#include "base/command_line.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/blocked_content_container.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/geolocation/geolocation_content_settings_map.h"
#include "chrome/browser/metrics/user_metrics.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_contents/tab_specific_content_settings.h"
#include "chrome/common/pref_names.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/browser/tab_contents/tab_contents_delegate.h"
#include "content/common/notification_service.h"
#include "grit/generated_resources.h"
#include "net/base/net_util.h"
#include "ui/base/l10n/l10n_util.h"

class ContentSettingTitleAndLinkModel : public ContentSettingBubbleModel {
 public:
  ContentSettingTitleAndLinkModel(TabContents* tab_contents,
                                  Profile* profile,
                                  ContentSettingsType content_type)
      : ContentSettingBubbleModel(tab_contents, profile, content_type) {
     // Notifications do not have a bubble.
     DCHECK_NE(content_type, CONTENT_SETTINGS_TYPE_NOTIFICATIONS);
     SetBlockedResources();
     SetTitle();
     SetManageLink();
  }

  virtual ~ContentSettingTitleAndLinkModel() {}

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
      0,  // Prerender does not have a bubble.
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
      0,  // Prerender does not have a bubble.
    };
    COMPILE_ASSERT(arraysize(kLinkIDs) == CONTENT_SETTINGS_NUM_TYPES,
                   Need_a_setting_for_every_content_settings_type);
    set_manage_link(l10n_util::GetStringUTF8(kLinkIDs[content_type()]));
  }

  virtual void OnManageLinkClicked() {
    if (tab_contents())
      tab_contents()->delegate()->ShowContentSettingsPage(content_type());
  }
};

class ContentSettingTitleLinkAndCustomModel
    : public ContentSettingTitleAndLinkModel {
 public:
  ContentSettingTitleLinkAndCustomModel(TabContents* tab_contents,
                                        Profile* profile,
                                        ContentSettingsType content_type)
      : ContentSettingTitleAndLinkModel(tab_contents, profile, content_type) {
    SetCustomLink();
  }

  virtual ~ContentSettingTitleLinkAndCustomModel() {}

 private:
  void SetCustomLink() {
    static const int kCustomIDs[] = {
      IDS_BLOCKED_COOKIES_INFO,
      0,  // Images do not have a custom link.
      0,  // Javascript doesn't have a custom link.
      IDS_BLOCKED_PLUGINS_LOAD_ALL,
      0,  // Popups do not have a custom link.
      0,  // Geolocation custom links are set within that class.
      0,  // Notifications do not have a bubble.
      0,  // Prerender does not have a bubble.
    };
    COMPILE_ASSERT(arraysize(kCustomIDs) == CONTENT_SETTINGS_NUM_TYPES,
                   Need_a_setting_for_every_content_settings_type);
    if (kCustomIDs[content_type()])
      set_custom_link(l10n_util::GetStringUTF8(kCustomIDs[content_type()]));
  }

  virtual void OnCustomLinkClicked() {}
};


class ContentSettingSingleRadioGroup
    : public ContentSettingTitleLinkAndCustomModel {
 public:
  ContentSettingSingleRadioGroup(TabContents* tab_contents,
                                 Profile* profile,
                                 ContentSettingsType content_type)
      : ContentSettingTitleLinkAndCustomModel(tab_contents, profile,
                                              content_type),
        block_setting_(CONTENT_SETTING_BLOCK),
        selected_item_(0) {
    SetRadioGroup();
  }

  virtual ~ContentSettingSingleRadioGroup() {
    if (selected_item_ != bubble_content().radio_group.default_item) {
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

 private:
  ContentSetting block_setting_;
  int selected_item_;

  // Initialize the radio group by setting the appropriate labels for the
  // content type and setting the default value based on the content setting.
  void SetRadioGroup() {
    GURL url = tab_contents()->GetURL();
    std::wstring display_host_wide;
    net::AppendFormattedHost(url,
        UTF8ToWide(profile()->GetPrefs()->GetString(prefs::kAcceptLanguages)),
        &display_host_wide, NULL, NULL);
    std::string display_host(WideToUTF8(display_host_wide));

    if (display_host.empty())
      display_host = url.spec();

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
      0,  // Prerender does not have a bubble.
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
      0,  // Prerender does not have a bubble.
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
      0,  // Prerender does not have a bubble.
    };
    COMPILE_ASSERT(arraysize(kBlockIDs) == CONTENT_SETTINGS_NUM_TYPES,
                   Need_a_setting_for_every_content_settings_type);
    std::string radio_block_label;
    radio_block_label = l10n_util::GetStringUTF8(kBlockIDs[content_type()]);

    radio_group.radio_items.push_back(radio_allow_label);
    radio_group.radio_items.push_back(radio_block_label);
    HostContentSettingsMap* map = profile()->GetHostContentSettingsMap();
    ContentSetting mostRestrictiveSetting;
    if (resources.empty()) {
      mostRestrictiveSetting =
          map->GetContentSetting(url, content_type(), std::string());
    } else {
      mostRestrictiveSetting = CONTENT_SETTING_ALLOW;
      for (std::set<std::string>::const_iterator it = resources.begin();
           it != resources.end(); ++it) {
        ContentSetting setting = map->GetContentSetting(url,
                                                        content_type(),
                                                        *it);
        if (setting == CONTENT_SETTING_BLOCK) {
          mostRestrictiveSetting = CONTENT_SETTING_BLOCK;
          break;
        }
        if (setting == CONTENT_SETTING_ASK)
          mostRestrictiveSetting = CONTENT_SETTING_ASK;
      }
    }
    if (mostRestrictiveSetting == CONTENT_SETTING_ALLOW) {
      radio_group.default_item = 0;
      // |block_setting_| is already set to |CONTENT_SETTING_BLOCK|.
    } else {
      radio_group.default_item = 1;
      block_setting_ = mostRestrictiveSetting;
    }
    selected_item_ = radio_group.default_item;
    set_radio_group(radio_group);
  }

  void AddException(ContentSetting setting,
                    const std::string& resource_identifier) {
    profile()->GetHostContentSettingsMap()->AddExceptionForURL(
        bubble_content().radio_group.url, content_type(), resource_identifier,
        setting);
  }

  virtual void OnRadioClicked(int radio_index) {
    selected_item_ = radio_index;
  }
};

class ContentSettingCookiesBubbleModel
    : public ContentSettingTitleLinkAndCustomModel {
 public:
  ContentSettingCookiesBubbleModel(TabContents* tab_contents,
                                   Profile* profile,
                                   ContentSettingsType content_type)
      : ContentSettingTitleLinkAndCustomModel(tab_contents, profile,
                                              content_type) {
    DCHECK_EQ(CONTENT_SETTINGS_TYPE_COOKIES, content_type);
    set_custom_link_enabled(true);
  }

  virtual ~ContentSettingCookiesBubbleModel() {}

 private:
  virtual void OnCustomLinkClicked() OVERRIDE {
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

class ContentSettingPluginBubbleModel : public ContentSettingSingleRadioGroup {
 public:
  ContentSettingPluginBubbleModel(TabContents* tab_contents,
                                  Profile* profile,
                                  ContentSettingsType content_type)
      : ContentSettingSingleRadioGroup(tab_contents, profile, content_type) {
    DCHECK_EQ(content_type, CONTENT_SETTINGS_TYPE_PLUGINS);
    set_custom_link_enabled(tab_contents && tab_contents->
        GetTabSpecificContentSettings()->load_plugins_link_enabled());
  }

  virtual ~ContentSettingPluginBubbleModel() {}

 private:
  virtual void OnCustomLinkClicked() OVERRIDE {
    UserMetrics::RecordAction(UserMetricsAction("ClickToPlay_LoadAll_Bubble"));
    DCHECK(tab_contents());
    tab_contents()->render_view_host()->LoadBlockedPlugins();
    set_custom_link_enabled(false);
    tab_contents()->GetTabSpecificContentSettings()->
        set_load_plugins_link_enabled(false);
  }
};

class ContentSettingPopupBubbleModel : public ContentSettingSingleRadioGroup {
 public:
  ContentSettingPopupBubbleModel(TabContents* tab_contents,
                                 Profile* profile,
                                 ContentSettingsType content_type)
      : ContentSettingSingleRadioGroup(tab_contents, profile, content_type) {
    SetPopups();
  }

  virtual ~ContentSettingPopupBubbleModel() {}

 private:
  void SetPopups() {
    // check for crbug.com/53176
    if (!tab_contents()->blocked_content_container())
      return;
    std::vector<TabContents*> blocked_contents;
    tab_contents()->blocked_content_container()->GetBlockedContents(
        &blocked_contents);
    for (std::vector<TabContents*>::const_iterator
         i(blocked_contents.begin()); i != blocked_contents.end(); ++i) {
      std::string title(UTF16ToUTF8((*i)->GetTitle()));
      // The popup may not have committed a load yet, in which case it won't
      // have a URL or title.
      if (title.empty())
        title = l10n_util::GetStringUTF8(IDS_TAB_LOADING_TITLE);
      PopupItem popup_item;
      popup_item.title = title;
      popup_item.bitmap = (*i)->GetFavicon();
      popup_item.tab_contents = (*i);
      add_popup(popup_item);
    }
  }

  virtual void OnPopupClicked(int index) {
    if (tab_contents() && tab_contents()->blocked_content_container()) {
      tab_contents()->blocked_content_container()->LaunchForContents(
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
    SetDomainsAndCustomLink();
  }

  virtual ~ContentSettingDomainListBubbleModel() {}

 private:
  void MaybeAddDomainList(const std::set<std::string>& hosts, int title_id) {
    if (!hosts.empty()) {
      DomainList domain_list;
      domain_list.title = l10n_util::GetStringUTF8(title_id);
      domain_list.hosts = hosts;
      add_domain_list(domain_list);
    }
  }
  void SetDomainsAndCustomLink() {
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
      set_custom_link(l10n_util::GetStringUTF8(
                      IDS_GEOLOCATION_BUBBLE_CLEAR_LINK));
      set_custom_link_enabled(true);
    } else if (tab_state_flags &
               GeolocationSettingsState::TABSTATE_HAS_CHANGED) {
      set_custom_link(l10n_util::GetStringUTF8(
                      IDS_GEOLOCATION_BUBBLE_REQUIRE_RELOAD_TO_CLEAR));
    }
  }
  virtual void OnCustomLinkClicked() OVERRIDE {
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
    return new ContentSettingCookiesBubbleModel(tab_contents, profile,
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
    TabContents* tab_contents,
    Profile* profile,
    ContentSettingsType content_type)
    : tab_contents_(tab_contents),
      profile_(profile),
      content_type_(content_type) {
  registrar_.Add(this, NotificationType::TAB_CONTENTS_DESTROYED,
                 Source<TabContents>(tab_contents));
}

ContentSettingBubbleModel::~ContentSettingBubbleModel() {
}

ContentSettingBubbleModel::RadioGroup::RadioGroup() : default_item(0) {}

ContentSettingBubbleModel::RadioGroup::~RadioGroup() {}

ContentSettingBubbleModel::DomainList::DomainList() {}

ContentSettingBubbleModel::DomainList::~DomainList() {}

ContentSettingBubbleModel::BubbleContent::BubbleContent()
    : custom_link_enabled(false) {
}

ContentSettingBubbleModel::BubbleContent::~BubbleContent() {}


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
