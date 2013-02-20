// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_CONTENT_SETTING_BUBBLE_CONTENTS_H_
#define CHROME_BROWSER_UI_VIEWS_CONTENT_SETTING_BUBBLE_CONTENTS_H_

#include <map>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/common/content_settings_types.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/common/media_stream_request.h"
#include "ui/views/bubble/bubble_delegate.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/menu_button_listener.h"
#include "ui/views/controls/link_listener.h"

class ContentSettingBubbleModel;
class ContentSettingMediaMenuModel;
class Profile;

namespace content {
class WebContents;
}

namespace ui {
class SimpleMenuModel;
}

namespace views {
class MenuButton;
class MenuRunner;
class TextButton;
class RadioButton;
}

// ContentSettingBubbleContents is used when the user turns on different kinds
// of content blocking (e.g. "block images").  When viewing a page with blocked
// content, icons appear in the omnibox corresponding to the content types that
// were blocked, and the user can click one to get a bubble hosting a few
// controls.  This class provides the content of that bubble.  In general,
// these bubbles typically have a title, a pair of radio buttons for toggling
// the blocking settings for the current site, a close button, and a link to
// get to a more comprehensive settings management dialog.  A few types have
// more or fewer controls than this.
class ContentSettingBubbleContents : public content::NotificationObserver,
                                     public views::BubbleDelegateView,
                                     public views::ButtonListener,
                                     public views::LinkListener,
                                     public views::MenuButtonListener {
 public:
  ContentSettingBubbleContents(
      ContentSettingBubbleModel* content_setting_bubble_model,
      content::WebContents* web_contents,
      views::View* anchor_view,
      views::BubbleBorder::ArrowLocation arrow_location);
  virtual ~ContentSettingBubbleContents();

  virtual gfx::Size GetPreferredSize() OVERRIDE;

  // Callback to allow ContentSettingMediaMenuModel to update the menu label.
  void UpdateMenuLabel(content::MediaStreamType type,
                       const std::string& label);

 protected:
  // views::BubbleDelegateView:
  virtual void Init() OVERRIDE;

 private:
  class Favicon;
  struct MediaMenuParts;

  typedef std::map<views::Link*, int> PopupLinks;
  typedef std::map<views::MenuButton*, MediaMenuParts*> MediaMenuPartsMap;

  // views::ButtonListener:
  virtual void ButtonPressed(views::Button* sender,
                             const ui::Event& event) OVERRIDE;

  // views::LinkListener:
  virtual void LinkClicked(views::Link* source, int event_flags) OVERRIDE;

  // views::MenuButtonListener:
  virtual void OnMenuButtonClicked(views::View* source,
                                   const gfx::Point& point) OVERRIDE;

  // content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Helper to get the preferred width of the media menu.
  int GetPreferredMediaMenuWidth(views::MenuButton* button,
                                 ui::SimpleMenuModel* menu_model);

  // Provides data for this bubble.
  scoped_ptr<ContentSettingBubbleModel> content_setting_bubble_model_;

  // The active web contents.
  content::WebContents* web_contents_;

  // A registrar for listening for WEB_CONTENTS_DESTROYED notifications.
  content::NotificationRegistrar registrar_;

  // Some of our controls, so we can tell what's been clicked when we get a
  // message.
  PopupLinks popup_links_;
  typedef std::vector<views::RadioButton*> RadioGroup;
  RadioGroup radio_group_;
  views::Link* custom_link_;
  views::Link* manage_link_;
  views::TextButton* close_button_;
  scoped_ptr<views::MenuRunner> menu_runner_;
  MediaMenuPartsMap media_menus_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(ContentSettingBubbleContents);
};

#endif  // CHROME_BROWSER_UI_VIEWS_CONTENT_SETTING_BUBBLE_CONTENTS_H_
