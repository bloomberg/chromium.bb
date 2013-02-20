// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_CONTENT_SETTING_BUBBLE_GTK_H_
#define CHROME_BROWSER_UI_GTK_CONTENT_SETTING_BUBBLE_GTK_H_

#include <map>

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/gtk/bubble/bubble_gtk.h"
#include "chrome/browser/ui/gtk/menu_gtk.h"
#include "chrome/common/content_settings_types.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/common/media_stream_request.h"
#include "ui/base/gtk/gtk_signal.h"
#include "ui/base/gtk/owned_widget_gtk.h"

class ContentSettingBubbleModel;
class ContentSettingMediaMenuModel;
class Profile;

namespace content {
class WebContents;
}

namespace ui {
class SimpleMenuModel;
}

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
       Profile* profile, content::WebContents* web_contents);
  virtual ~ContentSettingBubbleGtk();

  // Callback to allow ContentSettingMediaMenuModel to update the menu label.
  void UpdateMenuLabel(content::MediaStreamType type,
                       const std::string& label);

  // Dismisses the bubble.
  void Close();

 private:
  // A map from a GtkWidget* to a MediaMenuGtk*. MediaMenuGtk struct is used
  // to store the UI members that a media menu owns.
  struct MediaMenuGtk {
    explicit MediaMenuGtk(content::MediaStreamType type);
    ~MediaMenuGtk();

    content::MediaStreamType type;
    scoped_ptr<ui::SimpleMenuModel> menu_model;
    scoped_ptr<MenuGtk> menu;
    ui::OwnedWidgetGtk label;

   private:
    DISALLOW_COPY_AND_ASSIGN(MediaMenuGtk);
  };
  typedef std::map<GtkWidget*, MediaMenuGtk*> GtkMediaMenuMap;

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
  CHROMEGTK_CALLBACK_0(ContentSettingBubbleGtk, void, OnMenuButtonClicked);

  // We position the bubble near this widget.
  GtkWidget* anchor_;

  // The active profile.
  Profile* profile_;

  // The active web contents.
  content::WebContents* web_contents_;

  // A registrar for listening for WEB_CONTENTS_DESTROYED notifications.
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

  GtkMediaMenuMap media_menus_;
};

#endif  // CHROME_BROWSER_UI_GTK_CONTENT_SETTING_BUBBLE_GTK_H_
