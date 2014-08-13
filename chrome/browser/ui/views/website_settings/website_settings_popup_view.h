// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEBSITE_SETTINGS_WEBSITE_SETTINGS_POPUP_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_WEBSITE_SETTINGS_WEBSITE_SETTINGS_POPUP_VIEW_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "chrome/browser/ui/views/website_settings/permission_selector_view_observer.h"
#include "chrome/browser/ui/website_settings/website_settings_ui.h"
#include "content/public/common/signed_certificate_timestamp_id_and_status.h"
#include "ui/views/bubble/bubble_delegate.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/link_listener.h"
#include "ui/views/controls/tabbed_pane/tabbed_pane_listener.h"

class Browser;
class GURL;
class PermissionSelectorView;
class PopupHeaderView;
class Profile;

namespace content {
struct SSLStatus;
class WebContents;
}

namespace views {
class LabelButton;
class Link;
class TabbedPane;
class Widget;
}

// The views implementation of the website settings UI.
class WebsiteSettingsPopupView
    : public PermissionSelectorViewObserver,
      public views::BubbleDelegateView,
      public views::ButtonListener,
      public views::LinkListener,
      public views::TabbedPaneListener,
      public WebsiteSettingsUI {
 public:
  virtual ~WebsiteSettingsPopupView();

  static void ShowPopup(views::View* anchor_view,
                        Profile* profile,
                        content::WebContents* web_contents,
                        const GURL& url,
                        const content::SSLStatus& ssl,
                        Browser* browser);

  static bool IsPopupShowing();

 private:
  WebsiteSettingsPopupView(views::View* anchor_view,
                           Profile* profile,
                           content::WebContents* web_contents,
                           const GURL& url,
                           const content::SSLStatus& ssl,
                           Browser* browser);

  // PermissionSelectorViewObserver implementation.
  virtual void OnPermissionChanged(
      const WebsiteSettingsUI::PermissionInfo& permission) OVERRIDE;

  // views::BubbleDelegateView implementation.
  virtual void OnWidgetDestroying(views::Widget* widget) OVERRIDE;

  // views::ButtonListener implementation.
  virtual void ButtonPressed(views::Button* button,
                             const ui::Event& event) OVERRIDE;

  // views::LinkListener implementation.
  virtual void LinkClicked(views::Link* source, int event_flags) OVERRIDE;

  // views::TabbedPaneListener implementations.
  virtual void TabSelectedAt(int index) OVERRIDE;

  // views::View implementation.
  virtual gfx::Size GetPreferredSize() const OVERRIDE;

  // WebsiteSettingsUI implementations.
  virtual void SetCookieInfo(const CookieInfoList& cookie_info_list) OVERRIDE;
  virtual void SetPermissionInfo(
      const PermissionInfoList& permission_info_list) OVERRIDE;
  virtual void SetIdentityInfo(const IdentityInfo& identity_info) OVERRIDE;
  virtual void SetFirstVisit(const base::string16& first_visit) OVERRIDE;
  virtual void SetSelectedTab(TabId tab_id) OVERRIDE;

  // Creates the contents of the "Permissions" tab. The ownership of the
  // returned view is transferred to the caller.
  views::View* CreatePermissionsTab() WARN_UNUSED_RESULT;

  // Creates the contents of the "connection" tab. The ownership of the returned
  // view is transferred to the caller.
  views::View* CreateConnectionTab() WARN_UNUSED_RESULT;

  // Each tab contains several sections with a |headline| followed by the
  // section |contents| and an optional |link|. This method creates a section
  // for the given |headline|, |contents| and |link|. |link| can be NULL if the
  // section should not contain a link.
  views::View* CreateSection(const base::string16& headline,
                             views::View* contents,
                             views::Link* link) WARN_UNUSED_RESULT;

  // Resets the content of a section. All children of the |section_container|
  // are cleared and destroyed first. Then the |icon|, |headline|, |text| and
  // |link| are layout out properly. If the |headline| is an empty string then
  // no headline will be displayed. The ownership of the passed |link| is
  // transfered to the ResetConnectionSection method and the |link| is added to
  // the views hierarchy. If the |link| is NULL then no link is be displayed.
  void ResetConnectionSection(views::View* section_container,
                              const gfx::Image& icon,
                              const base::string16& headline,
                              const base::string16& text,
                              views::Link* link,
                              views::Link* secondary_link,
                              views::LabelButton* reset_decisions_button);

  // The web contents of the current tab. The popup can't live longer than a
  // tab.
  content::WebContents* web_contents_;

  // The Browser is used to load the help center page.
  Browser* browser_;

  // The presenter that controlls the Website Settings UI.
  scoped_ptr<WebsiteSettings> presenter_;

  PopupHeaderView* header_;  // Owned by views hierarchy.

  // The |TabbedPane| that contains the tabs of the Website Settings UI.
  views::TabbedPane* tabbed_pane_;

  // The view that contains the contents of the "Cookies and Site data" section
  // from the "Permissions" tab.
  views::View* site_data_content_;
  // The link that opend the "Cookies" dialog.
  views::Link* cookie_dialog_link_;
  // The view that contains the contents of the "Permissions" section from the
  // "Permissions" tab.
  views::View* permissions_content_;

  // The view that contains the connection tab contents.
  views::View* connection_tab_;
  // The view that contains the ui elements for displaying information about
  // the site's identity.
  views::View* identity_info_content_;
  // The link to open the certificate viewer for displaying the certificate
  // provided by the website. If the site does not provide a certificate then
  // |certificate_dialog_link_| is NULL.
  views::Link* certificate_dialog_link_;
  // The link to open the signed certificate timestamps viewer for displaying
  // Certificate Transparency info. If no such SCTs accompany the certificate
  // then |signed_certificate_timestamps_link_| is NULL.
  views::Link* signed_certificate_timestamps_link_;
  // The button to reset the Allow/Deny certificate errors decision for the
  // current host.
  views::LabelButton* reset_decisions_button_;

  // The id of the certificate provided by the site. If the site does not
  // provide a certificate then |cert_id_| is 0.
  int cert_id_;
  // The IDs and validation status of Signed Certificate TImestamps provided
  // by the site. Empty if no SCTs accompany the certificate.
  content::SignedCertificateTimestampIDStatusList
      signed_certificate_timestamp_ids_;

  // The link to open the help center page that contains more information about
  // the connection status icons.
  views::Link* help_center_link_;

  views::View* connection_info_content_;
  views::View* page_info_content_;

  base::WeakPtrFactory<WebsiteSettingsPopupView> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(WebsiteSettingsPopupView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_WEBSITE_SETTINGS_WEBSITE_SETTINGS_POPUP_VIEW_H_
