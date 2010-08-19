// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTENT_SETTING_BUBBLE_MODEL_H_
#define CHROME_BROWSER_CONTENT_SETTING_BUBBLE_MODEL_H_
#pragma once

#include <set>
#include <string>
#include <vector>

#include "chrome/common/content_settings.h"
#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_registrar.h"
#include "googleurl/src/gurl.h"
#include "third_party/skia/include/core/SkBitmap.h"

class Profile;
class SkBitmap;
class TabContents;

// This model provides data for ContentSettingBubble, and also controls
// the action triggered when the allow / block radio buttons are triggered.
class ContentSettingBubbleModel : public NotificationObserver {
 public:
  virtual ~ContentSettingBubbleModel();

  static ContentSettingBubbleModel* CreateContentSettingBubbleModel(
      TabContents* tab_contents,
      Profile* profile,
      ContentSettingsType content_type);

  ContentSettingsType content_type() const { return content_type_; }

  struct PopupItem {
    SkBitmap bitmap;
    std::string title;
    TabContents* tab_contents;
  };
  typedef std::vector<PopupItem> PopupItems;

  typedef std::vector<std::string> RadioItems;
  struct RadioGroup {
    GURL url;
    std::string title;
    RadioItems radio_items;
    int default_item;
  };

  struct DomainList {
    std::string title;
    std::set<std::string> hosts;
  };

  struct BubbleContent {
    std::string title;
    PopupItems popup_items;
    RadioGroup radio_group;
    std::vector<DomainList> domain_lists;
    std::set<std::string> resource_identifiers;
    std::string manage_link;
    std::string clear_link;
    std::string info_link;
    std::string load_plugins_link_title;
    bool load_plugins_link_enabled;
  };

  const BubbleContent& bubble_content() const { return bubble_content_; }

  // NotificationObserver:
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  virtual void OnRadioClicked(int radio_index) {}
  virtual void OnPopupClicked(int index) {}
  virtual void OnManageLinkClicked() {}
  virtual void OnClearLinkClicked() {}
  virtual void OnInfoLinkClicked() {}
  virtual void OnLoadPluginsLinkClicked() {}

 protected:
  ContentSettingBubbleModel(TabContents* tab_contents, Profile* profile,
      ContentSettingsType content_type);

  TabContents* tab_contents() const { return tab_contents_; }
  Profile* profile() const { return profile_; }

  void set_title(const std::string& title) { bubble_content_.title = title; }
  void add_popup(const PopupItem& popup) {
    bubble_content_.popup_items.push_back(popup);
  }
  void set_radio_group(const RadioGroup& radio_group) {
    bubble_content_.radio_group = radio_group;
  }
  void add_domain_list(const DomainList& domain_list) {
    bubble_content_.domain_lists.push_back(domain_list);
  }
  void set_manage_link(const std::string& link) {
    bubble_content_.manage_link = link;
  }
  void set_clear_link(const std::string& link) {
    bubble_content_.clear_link = link;
  }
  void set_info_link(const std::string& link) {
    bubble_content_.info_link = link;
  }
  void set_load_plugins_link_title(const std::string& title) {
    bubble_content_.load_plugins_link_title = title;
  }
  void set_load_plugins_link_enabled(bool enabled) {
    bubble_content_.load_plugins_link_enabled = enabled;
  }
  void AddBlockedResource(const std::string& resource_identifier);

 private:
  TabContents* tab_contents_;
  Profile* profile_;
  ContentSettingsType content_type_;
  BubbleContent bubble_content_;
  // A registrar for listening for TAB_CONTENTS_DESTROYED notifications.
  NotificationRegistrar registrar_;
};

#endif  // CHROME_BROWSER_CONTENT_SETTING_BUBBLE_MODEL_H_
