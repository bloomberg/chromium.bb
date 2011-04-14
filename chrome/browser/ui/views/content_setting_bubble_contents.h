// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_CONTENT_SETTING_BUBBLE_CONTENTS_H_
#define CHROME_BROWSER_UI_VIEWS_CONTENT_SETTING_BUBBLE_CONTENTS_H_
#pragma once

#include <map>

#include "chrome/common/content_settings_types.h"
#include "content/common/notification_observer.h"
#include "content/common/notification_registrar.h"
#include "views/controls/button/button.h"
#include "views/controls/link.h"

// ContentSettingBubbleContents is used when the user turns on different kinds
// of content blocking (e.g. "block images").  When viewing a page with blocked
// content, icons appear in the omnibox corresponding to the content types that
// were blocked, and the user can click one to get a bubble hosting a few
// controls.  This class provides the content of that bubble.  In general,
// these bubbles typically have a title, a pair of radio buttons for toggling
// the blocking settings for the current site, a close button, and a link to
// get to a more comprehensive settings management dialog.  A few types have
// more or fewer controls than this.

class Bubble;
class ContentSettingBubbleModel;
class Profile;
class TabContents;

namespace views {
class NativeButton;
class RadioButton;
}

class ContentSettingBubbleContents : public views::View,
                                     public views::ButtonListener,
                                     public views::LinkController,
                                     public NotificationObserver {
 public:
  ContentSettingBubbleContents(
      ContentSettingBubbleModel* content_setting_bubble_model,
      Profile* profile, TabContents* tab_contents);
  virtual ~ContentSettingBubbleContents();

  // Sets |bubble_|, so we can close the bubble if needed.  The caller owns
  // the bubble and must keep it alive.
  void set_bubble(Bubble* bubble) { bubble_ = bubble; }

  virtual gfx::Size GetPreferredSize();

 private:
  class Favicon;

  typedef std::map<views::Link*, int> PopupLinks;

  // Overridden from views::View:
  virtual void ViewHierarchyChanged(bool is_add, View* parent, View* child);

  // views::ButtonListener:
  virtual void ButtonPressed(views::Button* sender, const views::Event& event);

  // views::LinkController:
  virtual void LinkActivated(views::Link* source, int event_flags);

  // NotificationObserver:
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // Creates the child views.
  void InitControlLayout();

  // Provides data for this bubble.
  scoped_ptr<ContentSettingBubbleModel> content_setting_bubble_model_;

  // The active profile.
  Profile* profile_;

  // The active tab contents.
  TabContents* tab_contents_;

  // A registrar for listening for TAB_CONTENTS_DESTROYED notifications.
  NotificationRegistrar registrar_;

  // The Bubble holding us.
  Bubble* bubble_;

  // Some of our controls, so we can tell what's been clicked when we get a
  // message.
  PopupLinks popup_links_;
  typedef std::vector<views::RadioButton*> RadioGroup;
  RadioGroup radio_group_;
  views::Link* custom_link_;
  views::Link* manage_link_;
  views::NativeButton* close_button_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(ContentSettingBubbleContents);
};

#endif  // CHROME_BROWSER_UI_VIEWS_CONTENT_SETTING_BUBBLE_CONTENTS_H_
