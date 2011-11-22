// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_CONTENT_SETTING_BUBBLE_GTK_H_
#define CHROME_BROWSER_UI_GTK_CONTENT_SETTING_BUBBLE_GTK_H_
#pragma once

#include <map>

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/gtk/bubble/bubble_gtk.h"
#include "chrome/common/content_settings_types.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "ui/base/gtk/gtk_signal.h"

class ContentSettingBubbleModel;
class Profile;
class TabContents;

// ContentSettingBubbleGtk is used when the user turns on different kinds of
// content blocking (e.g. "block images"). An icon appears in the location bar,
// and when clicked, an instance of this class is created specialized for the
// type of content being blocked.
class ContentSettingBubbleGtk : public BubbleDelegateGtk,
                                public content::NotificationObserver {
 public:
   ContentSettingBubbleGtk(
       GtkWidget* anchor,
       BubbleDelegateGtk* delegate,
       ContentSettingBubbleModel* content_setting_bubble_model,
       Profile* profile, TabContents* tab_contents);
  virtual ~ContentSettingBubbleGtk();

  // Dismisses the bubble.
  void Close();

 private:
  typedef std::map<GtkWidget*, int> PopupMap;

  // BubbleDelegateGtk:
  virtual void BubbleClosing(BubbleGtk* bubble, bool closed_by_escape) OVERRIDE;

  // content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Builds the bubble and all the widgets that it displays.
  void BuildBubble();

  // Widget callback methods.
  CHROMEGTK_CALLBACK_1(ContentSettingBubbleGtk, void, OnPopupIconButtonPress,
                       GdkEventButton*);
  CHROMEGTK_CALLBACK_0(ContentSettingBubbleGtk, void, OnPopupLinkClicked);
  CHROMEGTK_CALLBACK_0(ContentSettingBubbleGtk, void, OnRadioToggled);
  CHROMEGTK_CALLBACK_0(ContentSettingBubbleGtk, void, OnCustomLinkClicked);
  CHROMEGTK_CALLBACK_0(ContentSettingBubbleGtk, void, OnManageLinkClicked);
  CHROMEGTK_CALLBACK_0(ContentSettingBubbleGtk, void, OnCloseButtonClicked);

  // We position the bubble near this widget.
  GtkWidget* anchor_;

  // The active profile.
  Profile* profile_;

  // The active tab contents.
  TabContents* tab_contents_;

  // A registrar for listening for TAB_CONTENTS_DESTROYED notifications.
  content::NotificationRegistrar registrar_;

  // Pass on delegate messages to this.
  BubbleDelegateGtk* delegate_;

  // Provides data for this bubble.
  scoped_ptr<ContentSettingBubbleModel> content_setting_bubble_model_;

  // The bubble.
  BubbleGtk* bubble_;

  // Stored controls so we can figure out what was clicked.
  PopupMap popup_links_;
  PopupMap popup_icons_;

  typedef std::vector<GtkWidget*> RadioGroupGtk;
  RadioGroupGtk radio_group_gtk_;
};

#endif  // CHROME_BROWSER_UI_GTK_CONTENT_SETTING_BUBBLE_GTK_H_
