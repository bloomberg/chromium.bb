// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEBSITE_SETTINGS_WEBSITE_SETTINGS_POPUP_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_WEBSITE_SETTINGS_WEBSITE_SETTINGS_POPUP_VIEW_H_

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "chrome/browser/ssl/security_state_model.h"
#include "chrome/browser/ui/views/website_settings/permission_selector_view_observer.h"
#include "chrome/browser/ui/website_settings/website_settings_ui.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/views/bubble/bubble_delegate.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/link_listener.h"
#include "ui/views/controls/styled_label_listener.h"
#include "ui/views/controls/tabbed_pane/tabbed_pane_listener.h"

class GURL;
class PopupHeaderView;
class Profile;

namespace content {
class WebContents;
}

namespace test {
class WebsiteSettingsPopupViewTestApi;
}

namespace views {
class LabelButton;
class Link;
class TabbedPane;
class Widget;
}

// The views implementation of the website settings UI.
class WebsiteSettingsPopupView : public content::WebContentsObserver,
                                 public PermissionSelectorViewObserver,
                                 public views::BubbleDelegateView,
                                 public views::ButtonListener,
                                 public views::LinkListener,
                                 public views::StyledLabelListener,
                                 public views::TabbedPaneListener,
                                 public WebsiteSettingsUI {
 public:
  ~WebsiteSettingsPopupView() override;

  // If |anchor_view| is null, |anchor_rect| is used to anchor the bubble.
  static void ShowPopup(views::View* anchor_view,
                        const gfx::Rect& anchor_rect,
                        Profile* profile,
                        content::WebContents* web_contents,
                        const GURL& url,
                        const SecurityStateModel::SecurityInfo& security_info);

  static bool IsPopupShowing();

 private:
  friend class test::WebsiteSettingsPopupViewTestApi;

  WebsiteSettingsPopupView(
      views::View* anchor_view,
      gfx::NativeView parent_window,
      Profile* profile,
      content::WebContents* web_contents,
      const GURL& url,
      const SecurityStateModel::SecurityInfo& security_info);

  // WebContentsObserver implementation.
  void RenderFrameDeleted(content::RenderFrameHost* render_frame_host) override;

  // PermissionSelectorViewObserver implementation.
  void OnPermissionChanged(
      const WebsiteSettingsUI::PermissionInfo& permission) override;

  // views::BubbleDelegateView implementation.
  void OnWidgetDestroying(views::Widget* widget) override;

  // views::ButtonListener implementation.
  void ButtonPressed(views::Button* button, const ui::Event& event) override;

  // views::LinkListener implementation.
  void LinkClicked(views::Link* source, int event_flags) override;

  // views::StyledLabelListener implementation.
  void StyledLabelLinkClicked(const gfx::Range& range,
                              int event_flags) override;

  // views::TabbedPaneListener implementations.
  void TabSelectedAt(int index) override;

  // views::View implementation.
  gfx::Size GetPreferredSize() const override;

  // WebsiteSettingsUI implementations.
  void SetCookieInfo(const CookieInfoList& cookie_info_list) override;
  void SetPermissionInfo(
      const PermissionInfoList& permission_info_list) override;
  void SetIdentityInfo(const IdentityInfo& identity_info) override;
  void SetSelectedTab(TabId tab_id) override;

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
  // are cleared and destroyed first. Then the |icon|, |headline|, |text|,
  // |link|, and |secondary_link| are laid out properly. If the |headline| is an
  // empty string then no headline will be displayed. Ownership of |link| and
  // |secondary_link| is transfered to the ResetConnectionSection method and the
  // links are added to the views hierarchy. NULL links are not displayed.
  void ResetConnectionSection(views::View* section_container,
                              const gfx::Image& icon,
                              const base::string16& headline,
                              const base::string16& text,
                              views::Link* link,
                              views::LabelButton* reset_decisions_button);

  // Used to asynchronously handle clicks since these calls may cause the
  // destruction of the settings view and the base class window still needs to
  // be alive to finish handling the mouse or keyboard click.
  void HandleLinkClickedAsync(views::Link* source);

  // The web contents of the current tab. The popup can't live longer than a
  // tab.
  content::WebContents* web_contents_;

  // The presenter that controls the Website Settings UI.
  scoped_ptr<WebsiteSettings> presenter_;

  PopupHeaderView* header_;  // Owned by views hierarchy.

  // The |TabbedPane| that contains the tabs of the Website Settings UI.
  views::TabbedPane* tabbed_pane_;

  // The view that contains the permissions tab contents.
  views::View* permissions_tab_;
  // The view that contains the contents of the "Cookies and Site data" section
  // from the "Permissions" tab.
  views::View* site_data_content_;
  // The link that opens the "Cookies" dialog.
  views::Link* cookie_dialog_link_;
  // The view that contains the contents of the "Permissions" section from the
  // "Permissions" tab.
  views::View* permissions_content_;

  // The view that contains the connection tab contents.
  views::View* connection_tab_;
  // The view that contains the UI elements for displaying information about
  // the site's identity.
  views::View* identity_info_content_;
  // The link to open the certificate viewer for displaying the certificate
  // provided by the website. If the site does not provide a certificate then
  // |certificate_dialog_link_| is NULL.
  views::Link* certificate_dialog_link_;
  // The button to reset the Allow/Deny certificate errors decision for the
  // current host.
  views::LabelButton* reset_decisions_button_;
  // The view that contains the contents of the "What Do These Mean?" section
  // from the "Connection" tab.
  views::View* help_center_content_;

  // The ID of the certificate provided by the site. If the site does not
  // provide a certificate then |cert_id_| is 0.
  int cert_id_;

  // The link to open the help center page that contains more information about
  // the connection status icons.
  views::Link* help_center_link_;

  // The link to open the site settings page that provides full control over
  // the origin's permissions.
  views::Link* site_settings_link_;

  views::View* connection_info_content_;

  base::WeakPtrFactory<WebsiteSettingsPopupView> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(WebsiteSettingsPopupView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_WEBSITE_SETTINGS_WEBSITE_SETTINGS_POPUP_VIEW_H_
