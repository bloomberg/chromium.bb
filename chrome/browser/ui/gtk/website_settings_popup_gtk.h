// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_WEBSITE_SETTINGS_POPUP_GTK_H_
#define CHROME_BROWSER_UI_GTK_WEBSITE_SETTINGS_POPUP_GTK_H_
#pragma once

#include <gtk/gtk.h>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/gtk/bubble/bubble_gtk.h"
#include "chrome/browser/ui/website_settings/website_settings_ui.h"

class Browser;
class GtkThemeService;
class GURL;
class Profile;
class TabContents;
class WebsiteSettings;

namespace content {
struct SSLStatus;
}

// GTK implementation of the website settings UI.
class WebsiteSettingsPopupGtk : public WebsiteSettingsUI,
                                public BubbleDelegateGtk {
 public:
  // Creates a |WebsiteSettingsPopupGtk| and displays the UI. The |url|
  // contains the omnibox URL of the currently active tab, |parent| contains
  // the currently active window, |profile| contains the currently active
  // profile and |ssl| contains the |SSLStatus| of the connection to the
  // website in the currently active tab that is wrapped by the
  // |tab_contents|.
  static void Show(gfx::NativeWindow parent,
                   Profile* profile,
                   TabContents* tab_contents,
                   const GURL& url,
                   const content::SSLStatus& ssl);

 private:
  WebsiteSettingsPopupGtk(gfx::NativeWindow parent,
                          Profile* profile,
                          TabContents* tab_contents,
                          const GURL& url,
                          const content::SSLStatus& ssl);

  // WebsiteSettingsUI implementations.
  virtual void SetCookieInfo(const CookieInfoList& cookie_info_list) OVERRIDE;
  virtual void SetPermissionInfo(
      const PermissionInfoList& permission_info_list) OVERRIDE;
  virtual void SetIdentityInfo(const IdentityInfo& identity_info) OVERRIDE;
  virtual void SetFirstVisit(const string16& first_visit) OVERRIDE;

  // BubbleDelegateGtk implementation.
  virtual void BubbleClosing(BubbleGtk* bubble, bool closed_by_escape) OVERRIDE;


  virtual ~WebsiteSettingsPopupGtk();

  // Layouts the different sections retrieved from the model.
  void InitContents();

  // Removes all children of |container|.
  void ClearContainer(GtkWidget* container);

  // Creates a label that contains the given |text| and has the given |width|.
  GtkWidget* CreateTextLabel(const std::string& text, int width);

  // Creates a popup section and returns a virtual box that contains the
  // section content.
  GtkWidget* CreateSection(std::string section_title,
                           GtkWidget* section_content);

  // Callbacks for the link buttons.
  CHROMEGTK_CALLBACK_0(WebsiteSettingsPopupGtk, void, OnCookiesLinkClicked);
  CHROMEGTK_CALLBACK_0(WebsiteSettingsPopupGtk, void, OnPermissionChanged);
  CHROMEGTK_CALLBACK_0(WebsiteSettingsPopupGtk, void,
                       OnPermissionsSettingsLinkClicked);
  CHROMEGTK_CALLBACK_1(WebsiteSettingsPopupGtk, void, OnComboBoxShown,
                       GParamSpec*);
  CHROMEGTK_CALLBACK_0(WebsiteSettingsPopupGtk, void, OnViewCertLinkClicked);

  // Parent window.
  GtkWindow* parent_;

  // The container that contains the content of the popup.
  GtkWidget* contents_;

  // The widget relative to which the popup is positioned.
  GtkWidget* anchor_;

  // Provides colors and stuff.
  GtkThemeService* theme_service_;

  // The popup bubble container.
  BubbleGtk* bubble_;

  Profile* profile_;

  TabContents* tab_contents_;

  // The browser object of the current window. This is needed to open the
  // settings page in a new tab.
  Browser* browser_;

  // For secure connection |cert_id_| is set to the ID of the server
  // certificate. For non secure connections |cert_id_| is 0.
  int cert_id_;

  // Container for the popup header content.
  GtkWidget* header_box_;

  // Container for the cookies and site data section content.
  GtkWidget* cookies_section_contents_;

  // Container for the permissions section content.
  GtkWidget* permissions_section_contents_;

  // Container for the identity tab content.
  GtkWidget* identity_tab_contents_;

  // Container for the information about the first visit date of the website.
  GtkWidget* first_visit_contents_;

  // The UI translates user actions to specific events and forwards them to the
  // |presenter_|. The |presenter_| handles these events and updates the UI.
  scoped_ptr<WebsiteSettings> presenter_;

  DISALLOW_COPY_AND_ASSIGN(WebsiteSettingsPopupGtk);
};

#endif  // CHROME_BROWSER_UI_GTK_WEBSITE_SETTINGS_POPUP_GTK_H_
