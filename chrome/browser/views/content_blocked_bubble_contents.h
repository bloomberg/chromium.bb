// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_CONTENT_BLOCKED_BUBBLE_CONTENTS_H_
#define CHROME_BROWSER_VIEWS_CONTENT_BLOCKED_BUBBLE_CONTENTS_H_

#include <map>
#include <string>

#include "chrome/common/content_settings_types.h"
#include "chrome/common/notification_registrar.h"
#include "views/controls/button/button.h"
#include "views/controls/link.h"

// ContentBlockedBubbleContents is used when the user turns on different kinds
// of content blocking (e.g. "block images").  When viewing a page with blocked
// content, icons appear in the omnibox corresponding to the content types that
// were blocked, and the user can click one to get a bubble hosting a few
// controls.  This class provides the content of that bubble.  In general,
// these bubbles typically have a title, a pair of radio buttons for toggling
// the blocking settings for the current site, a close button, and a link to
// get to a more comprehensive settings management dialog.  A few types have
// more or fewer controls than this.

class InfoBubble;
class Profile;
class TabContents;

namespace views {
class NativeButton;
class RadioButton;
}

class ContentBlockedBubbleContents : public views::View,
                                     public views::ButtonListener,
                                     public views::LinkController,
                                     public NotificationObserver {
 public:
  ContentBlockedBubbleContents(ContentSettingsType content_type,
                               const std::string& host,
                               const std::wstring& display_host,
                               Profile* profile,
                               TabContents* tab_contents);
  virtual ~ContentBlockedBubbleContents();

  // Sets |info_bubble_|, so we can close the bubble if needed.  The caller owns
  // the bubble and must keep it alive.
  void set_info_bubble(InfoBubble* info_bubble) { info_bubble_ = info_bubble; }

 private:
  typedef std::map<views::Link*, TabContents*> PopupLinks;

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

  // The type of content handled by this view.
  ContentSettingsType content_type_;

  // The hostname affected.
  std::string host_;
  std::wstring display_host_;

  // The active profile.
  Profile* profile_;

  // The active tab contents.
  TabContents* tab_contents_;

  // A registrar for listening for TAB_CONTENTS_DESTROYED notifications.
  NotificationRegistrar registrar_;

  // The InfoBubble holding us.
  InfoBubble* info_bubble_;

  // Some of our controls, so we can tell what's been clicked when we get a
  // message.
  PopupLinks popup_links_;
  views::RadioButton* allow_radio_;
  views::RadioButton* block_radio_;
  views::NativeButton* close_button_;
  views::Link* manage_link_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(ContentBlockedBubbleContents);
};

#endif  // CHROME_BROWSER_VIEWS_CONTENT_BLOCKED_BUBBLE_CONTENTS_H_
