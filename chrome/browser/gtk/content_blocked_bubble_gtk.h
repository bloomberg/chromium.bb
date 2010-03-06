// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GTK_CONTENT_BLOCKED_BUBBLE_GTK_H_
#define CHROME_BROWSER_GTK_CONTENT_BLOCKED_BUBBLE_GTK_H_

#include <map>
#include <string>

#include "chrome/browser/gtk/info_bubble_gtk.h"
#include "chrome/common/content_settings_types.h"
#include "chrome/common/notification_registrar.h"

class Profile;
class TabContents;

// ContentBlockedBubbleGtk is used when the user turns on different kinds of
// content blocking (e.g. "block images"). An icon appears in the location bar,
// and when clicked, an instance of this class is created specialized for the
// type of content being blocked.
class ContentBlockedBubbleGtk : public InfoBubbleGtkDelegate,
                                public NotificationObserver {
 public:
  ContentBlockedBubbleGtk(GtkWindow* toplevel_window,
                          const gfx::Rect& bounds,
                          InfoBubbleGtkDelegate* delegate,
                          ContentSettingsType content_type,
                          const std::string& host,
                          const std::wstring& display_host,
                          Profile* profile,
                          TabContents* tab_contents);
  virtual ~ContentBlockedBubbleGtk();

  // Dismisses the infobubble.
  void Close();

 private:
  typedef std::map<GtkWidget*, TabContents*> PopupMap;

  // InfoBubbleGtkDelegate:
  virtual void InfoBubbleClosing(InfoBubbleGtk* info_bubble,
                                 bool closed_by_escape);

  // NotificationObserver:
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // Builds the info bubble and all the widgets that it displays.
  void BuildBubble();

  // Launches a popup from a click on a widget.
  void LaunchPopup(TabContents* popup);

  // Widget callback methods.
  static void OnPopupIconButtonPress(GtkWidget* icon,
                                     GdkEventButton* event,
                                     ContentBlockedBubbleGtk* bubble);
  static void OnPopupLinkClicked(GtkWidget* button,
                                 ContentBlockedBubbleGtk* bubble);
  static void OnAllowBlockToggled(GtkWidget* widget,
                                  ContentBlockedBubbleGtk* bubble);
  static void OnCloseButtonClicked(GtkButton *button,
                                   ContentBlockedBubbleGtk* bubble);
  static void OnManageLinkClicked(GtkButton* button,
                                  ContentBlockedBubbleGtk* bubble);

  // A reference to the toplevel browser window, which we pass to the
  // InfoBubbleGtk implementation so it can tell the WM that it's a subwindow.
  GtkWindow* toplevel_window_;

  // Positioning information for the info bubble.
  gfx::Rect bounds_;

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

  // Pass on delegate messages to this.
  InfoBubbleGtkDelegate* delegate_;

  // The info bubble.
  InfoBubbleGtk* info_bubble_;

  // Stored controls so we can figure out what was clicked.
  PopupMap popup_links_;
  PopupMap popup_icons_;
  GtkWidget* allow_radio_;
  GtkWidget* block_radio_;
};

#endif  // CHROME_BROWSER_GTK_CONTENT_BLOCKED_BUBBLE_GTK_H_
