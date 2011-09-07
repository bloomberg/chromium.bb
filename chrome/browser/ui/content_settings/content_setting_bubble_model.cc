// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/content_settings/content_setting_bubble_model.h"

#include "base/command_line.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/favicon/favicon_tab_helper.h"
#include "chrome/browser/infobars/infobar_tab_helper.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/blocked_content/blocked_content_tab_helper.h"
#include "chrome/browser/ui/blocked_content/blocked_content_tab_helper_delegate.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/collected_cookies_infobar_delegate.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/render_messages.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/browser/tab_contents/tab_contents_delegate.h"
#include "content/browser/user_metrics.h"
#include "content/common/notification_service.h"
#include "grit/generated_resources.h"
#include "net/base/net_util.h"
#include "ui/base/l10n/l10n_util.h"

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

class ContentSettingTitleAndLinkModel : public ContentSettingBubbleModel {
 public:
  ContentSettingTitleAndLinkModel(Browser* browser,
                                  TabContentsWrapper* tab_contents,
                                  Profile* profile,
                                  ContentSettingsType content_type)
      : ContentSettingBubbleModel(tab_contents, profile, content_type),
        browser_(browser) {
     // Notifications do not have a bubble.
     DCHECK_NE(content_type, CONTENT_SETTINGS_TYPE_NOTIFICATIONS);
     SetBlockedResources();
     SetTitle();
     SetManageLink();
  }

  virtual ~ContentSettingTitleAndLinkModel() {}
  Browser* browser() const { return browser_; }

 private:
  void SetBlockedResources() {
    TabSpecificContentSettings* settings = tab_contents()->content_settings();
    const std::set<std::string>& resources = settings->BlockedResourcesForType(
        content_type());
    for (std::set<std::string>::const_iterator it = resources.begin();
        it != resources.end(); ++it) {
      AddBlockedResource(*it);
    }
  }

  void SetTitle() {
    static const ContentSettingsTypeIdEntry kBlockedTitleIDs[] = {
      {CONTENT_SETTINGS_TYPE_COOKIES, IDS_BLOCKED_COOKIES_TITLE},
      {CONTENT_SETTINGS_TYPE_IMAGES, IDS_BLOCKED_IMAGES_TITLE},
      {CONTENT_SETTINGS_TYPE_JAVASCRIPT, IDS_BLOCKED_JAVASCRIPT_TITLE},
      {CONTENT_SETTINGS_TYPE_PLUGINS, IDS_BLOCKED_PLUGINS_MESSAGE},
      {CONTENT_SETTINGS_TYPE_POPUPS, IDS_BLOCKED_POPUPS_TITLE},
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
    if (tab_contents() && tab_contents()->content_settings()->
            IsContentAccessed(content_type()) &&
        !tab_contents()->content_settings()->IsContentBlocked(content_type())) {
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

  void SetManageLink() {
    static const ContentSettingsTypeIdEntry kLinkIDs[] = {
      {CONTENT_SETTINGS_TYPE_COOKIES, IDS_BLOCKED_COOKIES_LINK},
      {CONTENT_SETTINGS_TYPE_IMAGES, IDS_BLOCKED_IMAGES_LINK},
      {CONTENT_SETTINGS_TYPE_JAVASCRIPT, IDS_BLOCKED_JAVASCRIPT_LINK},
      {CONTENT_SETTINGS_TYPE_PLUGINS, IDS_BLOCKED_PLUGINS_LINK},
      {CONTENT_SETTINGS_TYPE_POPUPS, IDS_BLOCKED_POPUPS_LINK},
      {CONTENT_SETTINGS_TYPE_GEOLOCATION, IDS_GEOLOCATION_BUBBLE_MANAGE_LINK},
    };
    set_manage_link(l10n_util::GetStringUTF8(
        GetIdForContentType(kLinkIDs, arraysize(kLinkIDs), content_type())));
  }

  virtual void OnManageLinkClicked() {
    if (browser_)
      browser_->ShowContentSettingsPage(content_type());
  }

  Browser* browser_;
};

class ContentSettingTitleLinkAndCustomModel
    : public ContentSettingTitleAndLinkModel {
 public:
  ContentSettingTitleLinkAndCustomModel(Browser* browser,
                                        TabContentsWrapper* tab_contents,
                                        Profile* profile,
                                        ContentSettingsType content_type)
      : ContentSettingTitleAndLinkModel(
            browser, tab_contents, profile, content_type) {
    SetCustomLink();
  }

  virtual ~ContentSettingTitleLinkAndCustomModel() {}

 private:
  void SetCustomLink() {
    static const ContentSettingsTypeIdEntry kCustomIDs[] = {
      {CONTENT_SETTINGS_TYPE_COOKIES, IDS_BLOCKED_COOKIES_INFO},
      {CONTENT_SETTINGS_TYPE_PLUGINS, IDS_BLOCKED_PLUGINS_LOAD_ALL},
    };
    int custom_link_id =
        GetIdForContentType(kCustomIDs, arraysize(kCustomIDs), content_type());
    if (custom_link_id)
      set_custom_link(l10n_util::GetStringUTF8(custom_link_id));
  }

  virtual void OnCustomLinkClicked() {}
};


class ContentSettingSingleRadioGroup
    : public ContentSettingTitleLinkAndCustomModel {
 public:
  ContentSettingSingleRadioGroup(Browser* browser,
                                 TabContentsWrapper* tab_contents,
                                 Profile* profile,
                                 ContentSettingsType content_type)
      : ContentSettingTitleLinkAndCustomModel(browser, tab_contents, profile,
                                              content_type),
        block_setting_(CONTENT_SETTING_BLOCK),
        selected_item_(0) {
    SetRadioGroup();
  }

  virtual ~ContentSettingSingleRadioGroup() {
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

 protected:
  bool settings_changed() const {
    return selected_item_ != bubble_content().radio_group.default_item;
  }

 private:
  ContentSetting block_setting_;
  int selected_item_;

  // Initialize the radio group by setting the appropriate labels for the
  // content type and setting the default value based on the content setting.
  void SetRadioGroup() {
    GURL url = tab_contents()->tab_contents()->GetURL();
    string16 display_host_utf16;
    net::AppendFormattedHost(url,
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
    ContentSetting mostRestrictiveSetting;
    if (resources.empty()) {
      mostRestrictiveSetting =
          content_type() == CONTENT_SETTINGS_TYPE_COOKIES ?
              map->GetCookieContentSetting(url, url, true) :
              map->GetContentSetting(url, url, content_type(), std::string());
    } else {
      mostRestrictiveSetting = CONTENT_SETTING_ALLOW;
      for (std::set<std::string>::const_iterator it = resources.begin();
           it != resources.end(); ++it) {
        ContentSetting setting = map->GetContentSetting(url,
                                                        url,
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
    if (profile()) {
      profile()->GetHostContentSettingsMap()->AddExceptionForURL(
          bubble_content().radio_group.url,
          bubble_content().radio_group.url,
          content_type(),
          resource_identifier,
          setting);
    }
  }

  virtual void OnRadioClicked(int radio_index) {
    selected_item_ = radio_index;
  }
};

class ContentSettingCookiesBubbleModel : public ContentSettingSingleRadioGroup {
 public:
  ContentSettingCookiesBubbleModel(Browser* browser,
                                   TabContentsWrapper* tab_contents,
                                   Profile* profile,
                                   ContentSettingsType content_type)
      : ContentSettingSingleRadioGroup(
            browser, tab_contents, profile, content_type) {
    DCHECK_EQ(CONTENT_SETTINGS_TYPE_COOKIES, content_type);
    set_custom_link_enabled(true);
  }

  virtual ~ContentSettingCookiesBubbleModel() {
    if (settings_changed()) {
      tab_contents()->infobar_tab_helper()->AddInfoBar(
          new CollectedCookiesInfoBarDelegate(tab_contents()->tab_contents()));
    }
  }

 private:
  virtual void OnCustomLinkClicked() OVERRIDE {
    if (!tab_contents())
      return;
    NotificationService::current()->Notify(
        chrome::NOTIFICATION_COLLECTED_COOKIES_SHOWN,
        Source<TabSpecificContentSettings>(tab_contents()->content_settings()),
        NotificationService::NoDetails());
    browser()->ShowCollectedCookiesDialog(tab_contents());
  }
};

class ContentSettingPluginBubbleModel : public ContentSettingSingleRadioGroup {
 public:
  ContentSettingPluginBubbleModel(Browser* browser,
                                  TabContentsWrapper* tab_contents,
                                  Profile* profile,
                                  ContentSettingsType content_type)
      : ContentSettingSingleRadioGroup(
            browser, tab_contents, profile, content_type) {
    DCHECK_EQ(content_type, CONTENT_SETTINGS_TYPE_PLUGINS);
    set_custom_link_enabled(tab_contents && tab_contents->content_settings()->
        load_plugins_link_enabled());
  }

  virtual ~ContentSettingPluginBubbleModel() {}

 private:
  virtual void OnCustomLinkClicked() OVERRIDE {
    UserMetrics::RecordAction(UserMetricsAction("ClickToPlay_LoadAll_Bubble"));
    DCHECK(tab_contents());
    RenderViewHost* host = tab_contents()->render_view_host();
    host->Send(new ChromeViewMsg_LoadBlockedPlugins(host->routing_id()));
    set_custom_link_enabled(false);
    tab_contents()->content_settings()->set_load_plugins_link_enabled(false);
  }
};

class ContentSettingPopupBubbleModel : public ContentSettingSingleRadioGroup {
 public:
  ContentSettingPopupBubbleModel(Browser* browser,
                                 TabContentsWrapper* tab_contents,
                                 Profile* profile,
                                 ContentSettingsType content_type)
      : ContentSettingSingleRadioGroup(
            browser, tab_contents, profile, content_type) {
    SetPopups();
  }

  virtual ~ContentSettingPopupBubbleModel() {}

 private:
  void SetPopups() {
    std::vector<TabContentsWrapper*> blocked_contents;
    tab_contents()->blocked_content_tab_helper()->
        GetBlockedContents(&blocked_contents);
    for (std::vector<TabContentsWrapper*>::const_iterator
         i = blocked_contents.begin(); i != blocked_contents.end(); ++i) {
      std::string title(UTF16ToUTF8((*i)->tab_contents()->GetTitle()));
      // The popup may not have committed a load yet, in which case it won't
      // have a URL or title.
      if (title.empty())
        title = l10n_util::GetStringUTF8(IDS_TAB_LOADING_TITLE);
      PopupItem popup_item;
      popup_item.title = title;
      popup_item.bitmap = (*i)->favicon_tab_helper()->GetFavicon();
      popup_item.tab_contents = (*i);
      add_popup(popup_item);
    }
  }

  virtual void OnPopupClicked(int index) {
    if (tab_contents()) {
      tab_contents()->blocked_content_tab_helper()->
          LaunchForContents(bubble_content().popup_items[index].tab_contents);
    }
  }
};

class ContentSettingDomainListBubbleModel
    : public ContentSettingTitleAndLinkModel {
 public:
  ContentSettingDomainListBubbleModel(Browser* browser,
                                      TabContentsWrapper* tab_contents,
                                      Profile* profile,
                                      ContentSettingsType content_type)
      : ContentSettingTitleAndLinkModel(
            browser, tab_contents, profile, content_type) {
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
        tab_contents()->content_settings();
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
    const GURL& embedder_url = tab_contents()->tab_contents()->GetURL();
    TabSpecificContentSettings* content_settings =
        tab_contents()->content_settings();
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
};

// static
ContentSettingBubbleModel*
    ContentSettingBubbleModel::CreateContentSettingBubbleModel(
        Browser* browser,
        TabContentsWrapper* tab_contents,
        Profile* profile,
        ContentSettingsType content_type) {
  if (content_type == CONTENT_SETTINGS_TYPE_COOKIES) {
    return new ContentSettingCookiesBubbleModel(browser, tab_contents, profile,
                                                content_type);
  }
  if (content_type == CONTENT_SETTINGS_TYPE_POPUPS) {
    return new ContentSettingPopupBubbleModel(browser, tab_contents, profile,
                                              content_type);
  }
  if (content_type == CONTENT_SETTINGS_TYPE_GEOLOCATION) {
    return new ContentSettingDomainListBubbleModel(browser, tab_contents,
                                                   profile, content_type);
  }
  if (content_type == CONTENT_SETTINGS_TYPE_PLUGINS) {
    return new ContentSettingPluginBubbleModel(browser, tab_contents, profile,
                                               content_type);
  }
  return new ContentSettingSingleRadioGroup(browser, tab_contents, profile,
                                            content_type);
}

ContentSettingBubbleModel::ContentSettingBubbleModel(
    TabContentsWrapper* tab_contents,
    Profile* profile,
    ContentSettingsType content_type)
    : tab_contents_(tab_contents),
      profile_(profile),
      content_type_(content_type) {
  registrar_.Add(this, content::NOTIFICATION_TAB_CONTENTS_DESTROYED,
                 Source<TabContents>(tab_contents->tab_contents()));
  registrar_.Add(this, chrome::NOTIFICATION_PROFILE_DESTROYED,
                 Source<Profile>(profile_));
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

void ContentSettingBubbleModel::Observe(int type,
                                        const NotificationSource& source,
                                        const NotificationDetails& details) {
  switch (type) {
    case content::NOTIFICATION_TAB_CONTENTS_DESTROYED:
      DCHECK(source == Source<TabContents>(tab_contents_->tab_contents()));
      tab_contents_ = NULL;
      break;
    case chrome::NOTIFICATION_PROFILE_DESTROYED:
      DCHECK(source == Source<Profile>(profile_));
      profile_ = NULL;
      break;
    default:
      NOTREACHED();
  }
}
