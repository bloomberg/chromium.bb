// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_setting_bubble_model.h"

#include "app/l10n_util.h"
#include "chrome/browser/blocked_popup_container.h"
#include "chrome/browser/geolocation/geolocation_content_settings_map.h"
#include "chrome/browser/host_content_settings_map.h"
#include "chrome/browser/pref_service.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/tab_contents/tab_contents_delegate.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/pref_names.h"
#include "grit/generated_resources.h"
#include "net/base/net_util.h"

class ContentSettingTitleAndLinkModel : public ContentSettingBubbleModel {
 public:
   ContentSettingTitleAndLinkModel(TabContents* tab_contents, Profile* profile,
      ContentSettingsType content_type)
      : ContentSettingBubbleModel(tab_contents, profile, content_type) {
     SetTitle();
     SetManageLink();
  }

 private:
  void SetTitle() {
    static const int kTitleIDs[] = {
      IDS_BLOCKED_COOKIES_TITLE,
      IDS_BLOCKED_IMAGES_TITLE,
      IDS_BLOCKED_JAVASCRIPT_TITLE,
      IDS_BLOCKED_PLUGINS_TITLE,
      IDS_BLOCKED_POPUPS_TITLE,
      0,  // Geolocation does not have an overall title.
    };
    COMPILE_ASSERT(arraysize(kTitleIDs) == CONTENT_SETTINGS_NUM_TYPES,
                   Need_a_setting_for_every_content_settings_type);
    if (kTitleIDs[content_type()])
      set_title(l10n_util::GetStringUTF8(kTitleIDs[content_type()]));
  }

  void SetManageLink() {
    static const int kLinkIDs[] = {
      IDS_BLOCKED_COOKIES_LINK,
      IDS_BLOCKED_IMAGES_LINK,
      IDS_BLOCKED_JAVASCRIPT_LINK,
      IDS_BLOCKED_PLUGINS_LINK,
      IDS_BLOCKED_POPUPS_LINK,
      IDS_GEOLOCATION_BUBBLE_MANAGE_LINK,
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

class ContentSettingSingleRadioGroup : public ContentSettingTitleAndLinkModel {
 public:
  ContentSettingSingleRadioGroup(TabContents* tab_contents, Profile* profile,
      ContentSettingsType content_type)
      : ContentSettingTitleAndLinkModel(tab_contents, profile, content_type) {
    SetRadioGroups();
  }

 private:
  void SetRadioGroups() {
    GURL url = tab_contents()->GetURL();
    std::wstring display_host_wide;
    net::AppendFormattedHost(url,
        profile()->GetPrefs()->GetString(prefs::kAcceptLanguages),
        &display_host_wide, NULL, NULL);
    std::string display_host(WideToUTF8(display_host_wide));

    RadioGroup radio_group;
    radio_group.host = url.host();

    static const int kAllowIDs[] = {
      0,  // We don't manage cookies here.
      IDS_BLOCKED_IMAGES_UNBLOCK,
      IDS_BLOCKED_JAVASCRIPT_UNBLOCK,
      IDS_BLOCKED_PLUGINS_UNBLOCK,
      IDS_BLOCKED_POPUPS_UNBLOCK,
      0,  // We don't manage geolocation here.
    };
    COMPILE_ASSERT(arraysize(kAllowIDs) == CONTENT_SETTINGS_NUM_TYPES,
                   Need_a_setting_for_every_content_settings_type);
    std::string radio_allow_label;
    radio_allow_label = l10n_util::GetStringFUTF8(
        kAllowIDs[content_type()], UTF8ToUTF16(display_host));

    static const int kBlockIDs[] = {
      0,  // We don't manage cookies here.
      IDS_BLOCKED_IMAGES_NO_ACTION,
      IDS_BLOCKED_JAVASCRIPT_NO_ACTION,
      IDS_BLOCKED_PLUGINS_NO_ACTION,
      IDS_BLOCKED_POPUPS_NO_ACTION,
      0,  // We don't manage geolocation here.
    };
    COMPILE_ASSERT(arraysize(kBlockIDs) == CONTENT_SETTINGS_NUM_TYPES,
                   Need_a_setting_for_every_content_settings_type);
    std::string radio_block_label;
    radio_block_label = l10n_util::GetStringFUTF8(
        kBlockIDs[content_type()], UTF8ToUTF16(display_host));

    radio_group.radio_items.push_back(radio_allow_label);
    radio_group.radio_items.push_back(radio_block_label);
    radio_group.default_item =
        profile()->GetHostContentSettingsMap()->GetContentSetting(url,
            content_type()) == CONTENT_SETTING_ALLOW ? 0 : 1;
    add_radio_group(radio_group);
  }

  virtual void OnRadioClicked(int radio_group, int radio_index) {
    profile()->GetHostContentSettingsMap()->SetContentSetting(
        bubble_content().radio_groups[radio_group].host,
        content_type(),
        radio_index == 0 ? CONTENT_SETTING_ALLOW : CONTENT_SETTING_BLOCK);
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
    BlockedPopupContainer::BlockedContents blocked_contents;
    DCHECK(tab_contents()->blocked_popup_container());
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
    SetDomains();
    SetClearLink();
  }

 private:
  void MaybeAddDomainList(DomainList* domain_list, int title_id) {
    if (!domain_list->hosts.empty()) {
      domain_list->title = l10n_util::GetStringUTF8(title_id);
      add_domain_list(*domain_list);
    }
  }
  void SetDomains() {
    const TabContents::GeolocationContentSettings& settings =
        tab_contents()->geolocation_content_settings();

    // Divide the tab's current geolocation users into sets according to their
    // permission state.
    DomainList domains[CONTENT_SETTING_NUM_SETTINGS];
    for (TabContents::GeolocationContentSettings::const_iterator it =
        settings.begin(); it != settings.end(); ++it) {
      domains[it->second].hosts.insert(it->first.host());
    }
    MaybeAddDomainList(&domains[CONTENT_SETTING_ALLOW],
                       IDS_GEOLOCATION_BUBBLE_SECTION_ALLOWED);
    MaybeAddDomainList(&domains[CONTENT_SETTING_BLOCK],
                       IDS_GEOLOCATION_BUBBLE_SECTION_DENIED);
  }
  void SetClearLink() {
    set_clear_link(l10n_util::GetStringUTF8(IDS_GEOLOCATION_BUBBLE_CLEAR_LINK));
  }
  virtual void OnClearLinkClicked() {
    if (!tab_contents())
      return;
    // Reset this embedder's entry to default for each of the requesting
    // origins currently on the page.
    const GURL& embedder_url = tab_contents()->GetURL();
    const TabContents::GeolocationContentSettings& settings =
        tab_contents()->geolocation_content_settings();
    GeolocationContentSettingsMap* settings_map =
        profile()->GetGeolocationContentSettingsMap();
    for (TabContents::GeolocationContentSettings::const_iterator it =
        settings.begin(); it != settings.end(); ++it) {
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
    return new ContentSettingTitleAndLinkModel(tab_contents, profile,
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
  return new ContentSettingSingleRadioGroup(tab_contents, profile,
                                            content_type);
}

ContentSettingBubbleModel::ContentSettingBubbleModel(
    TabContents* tab_contents, Profile* profile,
    ContentSettingsType content_type)
    : tab_contents_(tab_contents), profile_(profile),
      content_type_(content_type) {
  registrar_.Add(this, NotificationType::TAB_CONTENTS_DESTROYED,
                 Source<TabContents>(tab_contents));
}

ContentSettingBubbleModel::~ContentSettingBubbleModel() {
}

void ContentSettingBubbleModel::Observe(NotificationType type,
                                        const NotificationSource& source,
                                        const NotificationDetails& details) {
  DCHECK(type == NotificationType::TAB_CONTENTS_DESTROYED);
  DCHECK(source == Source<TabContents>(tab_contents_));
  tab_contents_ = NULL;
}
