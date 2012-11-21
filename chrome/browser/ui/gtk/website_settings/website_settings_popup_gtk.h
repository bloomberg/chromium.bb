// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_WEBSITE_SETTINGS_WEBSITE_SETTINGS_POPUP_GTK_H_
#define CHROME_BROWSER_UI_GTK_WEBSITE_SETTINGS_WEBSITE_SETTINGS_POPUP_GTK_H_

#include <gtk/gtk.h>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "chrome/browser/ui/gtk/bubble/bubble_gtk.h"
#include "chrome/browser/ui/gtk/website_settings/permission_selector_observer.h"
#include "chrome/browser/ui/website_settings/website_settings_ui.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class Browser;
class CustomDrawButton;
class GtkThemeService;
class GURL;
class PermissionSelector;
class Profile;
class WebsiteSettings;

namespace content {
struct SSLStatus;
class WebContents;
}

// GTK implementation of the website settings UI.
class WebsiteSettingsPopupGtk : public WebsiteSettingsUI,
                                public PermissionSelectorObserver,
                                public BubbleDelegateGtk,
                                public content::NotificationObserver {
 public:
  // Creates a |WebsiteSettingsPopupGtk| and displays the UI. The |url|
  // contains the omnibox URL of the currently active tab, |parent| contains
  // the currently active window, |profile| contains the currently active
  // profile and |ssl| contains the |SSLStatus| of the connection to the
  // website in the currently active tab that is wrapped by the
  // |web_contents|.
  static void Show(gfx::NativeWindow parent,
                   Profile* profile,
                   content::WebContents* web_contents,
                   const GURL& url,
                   const content::SSLStatus& ssl);

  virtual ~WebsiteSettingsPopupGtk();

 private:
  WebsiteSettingsPopupGtk(gfx::NativeWindow parent,
                          Profile* profile,
                          content::WebContents* web_contents,
                          const GURL& url,
                          const content::SSLStatus& ssl);

  // WebsiteSettingsUI implementations.
  virtual void SetCookieInfo(const CookieInfoList& cookie_info_list) OVERRIDE;
  virtual void SetPermissionInfo(
      const PermissionInfoList& permission_info_list) OVERRIDE;
  virtual void SetIdentityInfo(const IdentityInfo& identity_info) OVERRIDE;
  virtual void SetFirstVisit(const string16& first_visit) OVERRIDE;
  virtual void SetSelectedTab(WebsiteSettingsUI::TabId tab_id) OVERRIDE;

  // PermissionSelectorObserver implementations.
  virtual void OnPermissionChanged(PermissionSelector* selector) OVERRIDE;

  // BubbleDelegateGtk implementation.
  virtual void BubbleClosing(BubbleGtk* bubble, bool closed_by_escape) OVERRIDE;

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Layouts the different sections retrieved from the model.
  void InitContents();

  // Creates a tab for the tabstrip with label indicated by |ids|. This tab
  // is not used in GTK theme mode.
  GtkWidget* BuildTab(int ids);

  // Gives the index in the notebook for the given tab widget.
  int TabstripButtonToTabIndex(GtkWidget* tab);

  // Callbacks for the link buttons.
  CHROMEGTK_CALLBACK_1(WebsiteSettingsPopupGtk, gboolean,
                       OnTabButtonPress, GdkEvent*);
  CHROMEGTK_CALLBACK_1(WebsiteSettingsPopupGtk, gboolean,
                       OnTabExpose, GdkEventExpose*);
  CHROMEGTK_CALLBACK_1(WebsiteSettingsPopupGtk, gboolean,
                       OnTabstripExpose, GdkEventExpose*);
  CHROMEGTK_CALLBACK_0(WebsiteSettingsPopupGtk, void, OnCookiesLinkClicked);
  CHROMEGTK_CALLBACK_0(WebsiteSettingsPopupGtk, void, OnViewCertLinkClicked);
  CHROMEGTK_CALLBACK_0(WebsiteSettingsPopupGtk, void, OnCloseButtonClicked);
  CHROMEGTK_CALLBACK_0(WebsiteSettingsPopupGtk, void, OnHelpLinkClicked);

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

  content::WebContents* web_contents_;

  // The browser object of the current window. This is needed to open the
  // settings page in a new tab.
  Browser* browser_;

  // For secure connection |cert_id_| is set to the ID of the server
  // certificate. For non secure connections |cert_id_| is 0.
  int cert_id_;

  // Container for the popup header content.
  GtkWidget* header_box_;

  // Close button for the bubble.
  scoped_ptr<CustomDrawButton> close_button_;

  // Container for the cookies and site data section content.
  GtkWidget* cookies_section_contents_;

  // Container for the permissions section content.
  GtkWidget* permissions_section_contents_;

  // Container for the remote host (website) identity section of the connection
  // tab.
  GtkWidget* identity_contents_;

  // Container for the connection section of the connection tab.
  GtkWidget* connection_contents_;

  // Container for the information about the first visit date of the website.
  GtkWidget* first_visit_contents_;

  // The widget that wraps the tabstrip.
  GtkWidget* tabstrip_alignment_;

  // The widget that contains the tabs display on the popup.
  GtkWidget* notebook_;

  // The UI translates user actions to specific events and forwards them to the
  // |presenter_|. The |presenter_| handles these events and updates the UI.
  scoped_ptr<WebsiteSettings> presenter_;

  // The permission selectors that allow the user to change individual
  // permissions.
  ScopedVector<PermissionSelector> selectors_;

  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(WebsiteSettingsPopupGtk);
};

#endif  // CHROME_BROWSER_UI_GTK_WEBSITE_SETTINGS_WEBSITE_SETTINGS_POPUP_GTK_H_
