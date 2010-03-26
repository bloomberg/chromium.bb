// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GTK_CONTENT_BLOCKED_BUBBLE_GTK_H_
#define CHROME_BROWSER_GTK_CONTENT_BLOCKED_BUBBLE_GTK_H_

#include <map>
#include <string>

#include "base/scoped_ptr.h"
#include "chrome/browser/gtk/info_bubble_gtk.h"
#include "chrome/common/content_settings_types.h"
#include "chrome/common/notification_registrar.h"

class ContentSettingBubbleModel;
class Profile;
class TabContents;

// ContentSettingBubbleGtk is used when the user turns on different kinds of
// content blocking (e.g. "block images"). An icon appears in the location bar,
// and when clicked, an instance of this class is created specialized for the
// type of content being blocked.
// TODO(bulach): rename this file.
class ContentSettingBubbleGtk : public InfoBubbleGtkDelegate,
                                public NotificationObserver {
 public:
   ContentSettingBubbleGtk(
       GtkWindow* toplevel_window,
       const gfx::Rect& bounds,
       InfoBubbleGtkDelegate* delegate,
       ContentSettingBubbleModel* content_setting_bubble_model,
       Profile* profile, TabContents* tab_contents);
  virtual ~ContentSettingBubbleGtk();

  // Dismisses the infobubble.
  void Close();

 private:
  typedef std::map<GtkWidget*, int> PopupMap;

  // InfoBubbleGtkDelegate:
  virtual void InfoBubbleClosing(InfoBubbleGtk* info_bubble,
                                 bool closed_by_escape);

  // NotificationObserver:
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // Builds the info bubble and all the widgets that it displays.
  void BuildBubble();

  // Widget callback methods.
  static void OnPopupIconButtonPress(GtkWidget* icon,
                                     GdkEventButton* event,
                                     ContentSettingBubbleGtk* bubble);
  static void OnPopupLinkClicked(GtkWidget* button,
                                 ContentSettingBubbleGtk* bubble);
  static void OnRadioToggled(GtkWidget* widget,
                             ContentSettingBubbleGtk* bubble);
  static void OnCloseButtonClicked(GtkButton *button,
                                   ContentSettingBubbleGtk* bubble);
  static void OnManageLinkClicked(GtkButton* button,
                                  ContentSettingBubbleGtk* bubble);
  static void OnClearLinkClicked(GtkButton* button,
                                 ContentSettingBubbleGtk* bubble);

  // A reference to the toplevel browser window, which we pass to the
  // InfoBubbleGtk implementation so it can tell the WM that it's a subwindow.
  GtkWindow* toplevel_window_;

  // Positioning information for the info bubble.
  gfx::Rect bounds_;

  // The active profile.
  Profile* profile_;

  // The active tab contents.
  TabContents* tab_contents_;

  // A registrar for listening for TAB_CONTENTS_DESTROYED notifications.
  NotificationRegistrar registrar_;

  // Pass on delegate messages to this.
  InfoBubbleGtkDelegate* delegate_;

  // Provides data for this bubble.
  scoped_ptr<ContentSettingBubbleModel> content_setting_bubble_model_;

  // The info bubble.
  InfoBubbleGtk* info_bubble_;

  // Stored controls so we can figure out what was clicked.
  PopupMap popup_links_;
  PopupMap popup_icons_;

  typedef std::vector<GtkWidget*> RadioGroupGtk;
  std::vector<RadioGroupGtk> radio_groups_gtk_;
};

#endif  // CHROME_BROWSER_GTK_CONTENT_BLOCKED_BUBBLE_GTK_H_
