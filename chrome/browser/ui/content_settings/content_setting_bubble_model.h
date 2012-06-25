// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_CONTENT_SETTINGS_CONTENT_SETTING_BUBBLE_MODEL_H_
#define CHROME_BROWSER_UI_CONTENT_SETTINGS_CONTENT_SETTING_BUBBLE_MODEL_H_
#pragma once

#include <set>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "chrome/common/content_settings.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "googleurl/src/gurl.h"
#include "third_party/skia/include/core/SkBitmap.h"

class ContentSettingBubbleModelDelegate;
class Profile;
class TabContents;

// This model provides data for ContentSettingBubble, and also controls
// the action triggered when the allow / block radio buttons are triggered.
class ContentSettingBubbleModel : public content::NotificationObserver {
 public:
  typedef ContentSettingBubbleModelDelegate Delegate;

  struct PopupItem {
    SkBitmap bitmap;
    std::string title;
    TabContents* tab_contents;
  };
  typedef std::vector<PopupItem> PopupItems;

  typedef std::vector<std::string> RadioItems;
  struct RadioGroup {
    RadioGroup();
    ~RadioGroup();

    GURL url;
    std::string title;
    RadioItems radio_items;
    int default_item;
  };

  struct DomainList {
    DomainList();
    ~DomainList();

    std::string title;
    std::set<std::string> hosts;
  };

  struct BubbleContent {
    BubbleContent();
    ~BubbleContent();

    std::string title;
    PopupItems popup_items;
    RadioGroup radio_group;
    bool radio_group_enabled;
    std::vector<DomainList> domain_lists;
    std::set<std::string> resource_identifiers;
    std::string custom_link;
    bool custom_link_enabled;
    std::string manage_link;

   private:
    DISALLOW_COPY_AND_ASSIGN(BubbleContent);
  };

  static ContentSettingBubbleModel* CreateContentSettingBubbleModel(
      Delegate* delegate,
      TabContents* tab_contents,
      Profile* profile,
      ContentSettingsType content_type);

  virtual ~ContentSettingBubbleModel();

  ContentSettingsType content_type() const { return content_type_; }

  const BubbleContent& bubble_content() const { return bubble_content_; }

  // content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  virtual void OnRadioClicked(int radio_index) {}
  virtual void OnPopupClicked(int index) {}
  virtual void OnCustomLinkClicked() {}
  virtual void OnManageLinkClicked() {}

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
  void set_radio_group_enabled(bool enabled) {
    bubble_content_.radio_group_enabled = enabled;
  }
  void add_domain_list(const DomainList& domain_list) {
    bubble_content_.domain_lists.push_back(domain_list);
  }
  void set_custom_link(const std::string& link) {
    bubble_content_.custom_link = link;
  }
  void set_custom_link_enabled(bool enabled) {
    bubble_content_.custom_link_enabled = enabled;
  }
  void set_manage_link(const std::string& link) {
    bubble_content_.manage_link = link;
  }
  void AddBlockedResource(const std::string& resource_identifier);

 private:
  TabContents* tab_contents_;
  Profile* profile_;
  ContentSettingsType content_type_;
  BubbleContent bubble_content_;
  // A registrar for listening for TAB_CONTENTS_DESTROYED notifications.
  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(ContentSettingBubbleModel);
};

#endif  // CHROME_BROWSER_UI_CONTENT_SETTINGS_CONTENT_SETTING_BUBBLE_MODEL_H_
