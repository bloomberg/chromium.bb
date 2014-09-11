// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_CONTENT_SETTINGS_CONTENT_SETTING_BUBBLE_MODEL_H_
#define CHROME_BROWSER_UI_CONTENT_SETTINGS_CONTENT_SETTING_BUBBLE_MODEL_H_

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/common/custom_handlers/protocol_handler.h"
#include "components/content_settings/core/common/content_settings.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/common/media_stream_request.h"
#include "ui/gfx/image/image.h"
#include "url/gurl.h"

class ContentSettingBubbleModelDelegate;
class Profile;
class ProtocolHandlerRegistry;

namespace content {
class WebContents;
}

// This model provides data for ContentSettingBubble, and also controls
// the action triggered when the allow / block radio buttons are triggered.
class ContentSettingBubbleModel : public content::NotificationObserver {
 public:
  typedef ContentSettingBubbleModelDelegate Delegate;

  struct PopupItem {
    PopupItem(const gfx::Image& image, const std::string& title, int32 popup_id)
        : image(image), title(title), popup_id(popup_id) {}

    gfx::Image image;
    std::string title;
    int32 popup_id;
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

  struct MediaMenu {
    MediaMenu();
    ~MediaMenu();

    std::string label;
    content::MediaStreamDevice default_device;
    content::MediaStreamDevice selected_device;
    bool disabled;
  };
  typedef std::map<content::MediaStreamType, MediaMenu> MediaMenuMap;

  struct BubbleContent {
    BubbleContent();
    ~BubbleContent();

    std::string title;
    PopupItems popup_items;
    RadioGroup radio_group;
    bool radio_group_enabled;
    std::vector<DomainList> domain_lists;
    std::string custom_link;
    bool custom_link_enabled;
    std::string manage_link;
    MediaMenuMap media_menus;
    std::string learn_more_link;

   private:
    DISALLOW_COPY_AND_ASSIGN(BubbleContent);
  };

  static ContentSettingBubbleModel* CreateContentSettingBubbleModel(
      Delegate* delegate,
      content::WebContents* web_contents,
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
  virtual void OnLearnMoreLinkClicked() {}
  virtual void OnMediaMenuClicked(content::MediaStreamType type,
                                  const std::string& selected_device_id) {}

  // Called by the view code when the bubble is closed by the user using the
  // Done button.
  virtual void OnDoneClicked() {}

 protected:
  ContentSettingBubbleModel(
      content::WebContents* web_contents,
      Profile* profile,
      ContentSettingsType content_type);

  content::WebContents* web_contents() const { return web_contents_; }
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
  void set_learn_more_link(const std::string& link) {
    bubble_content_.learn_more_link = link;
  }
  void add_media_menu(content::MediaStreamType type, const MediaMenu& menu) {
    bubble_content_.media_menus[type] = menu;
  }
  void set_selected_device(const content::MediaStreamDevice& device) {
    bubble_content_.media_menus[device.type].selected_device = device;
  }
  bool setting_is_managed() {
    return setting_is_managed_;
  }
  void set_setting_is_managed(bool managed) {
    setting_is_managed_ = managed;
  }

 private:
  content::WebContents* web_contents_;
  Profile* profile_;
  ContentSettingsType content_type_;
  BubbleContent bubble_content_;
  // A registrar for listening for WEB_CONTENTS_DESTROYED notifications.
  content::NotificationRegistrar registrar_;
  // A flag that indicates if the content setting managed i.e. can't be
  // controlled by the user.
  bool setting_is_managed_;

  DISALLOW_COPY_AND_ASSIGN(ContentSettingBubbleModel);
};

class ContentSettingTitleAndLinkModel : public ContentSettingBubbleModel {
 public:
  ContentSettingTitleAndLinkModel(Delegate* delegate,
                                  content::WebContents* web_contents,
                                  Profile* profile,
                                  ContentSettingsType content_type);
  virtual ~ContentSettingTitleAndLinkModel() {}
  Delegate* delegate() const { return delegate_; }

 private:
  void SetTitle();
  void SetManageLink();
  void SetLearnMoreLink();

  // content::ContentSettingBubbleModel:
  virtual void OnManageLinkClicked() OVERRIDE;
  virtual void OnLearnMoreLinkClicked() OVERRIDE;
  Delegate* delegate_;
};

class ContentSettingRPHBubbleModel : public ContentSettingTitleAndLinkModel {
 public:
  ContentSettingRPHBubbleModel(Delegate* delegate,
                               content::WebContents* web_contents,
                               Profile* profile,
                               ProtocolHandlerRegistry* registry,
                               ContentSettingsType content_type);

  virtual void OnRadioClicked(int radio_index) OVERRIDE;
  virtual void OnDoneClicked() OVERRIDE;

 private:
  // These states must match the order of appearance of the radio buttons
  // in the XIB file for the Mac port.
  enum RPHState {
    RPH_ALLOW = 0,
    RPH_BLOCK,
    RPH_IGNORE,
  };

  void RegisterProtocolHandler();
  void UnregisterProtocolHandler();
  void IgnoreProtocolHandler();
  void ClearOrSetPreviousHandler();

  int selected_item_;
  ProtocolHandlerRegistry* registry_;
  ProtocolHandler pending_handler_;
  ProtocolHandler previous_handler_;
};

#endif  // CHROME_BROWSER_UI_CONTENT_SETTINGS_CONTENT_SETTING_BUBBLE_MODEL_H_
