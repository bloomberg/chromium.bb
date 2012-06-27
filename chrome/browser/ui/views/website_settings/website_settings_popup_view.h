// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEBSITE_SETTINGS_WEBSITE_SETTINGS_POPUP_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_WEBSITE_SETTINGS_WEBSITE_SETTINGS_POPUP_VIEW_H_
#pragma once

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/website_settings/website_settings_ui.h"
#include "chrome/browser/ui/views/website_settings/permission_selector_view_observer.h"
#include "ui/views/bubble/bubble_delegate.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/link_listener.h"
#include "ui/views/controls/tabbed_pane/tabbed_pane_listener.h"

class GURL;
class PermissionSelectorView;
class PopupHeaderView;
class Profile;
class TabContents;

namespace content {
struct SSLStatus;
}

namespace views {
class Link;
class TabbedPane;
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
                        TabContents* tab_contents,
                        const GURL& gurl,
                        const content::SSLStatus& ssl);

 private:
  WebsiteSettingsPopupView(views::View* anchor_view,
                           Profile* profile,
                           TabContents* tab_contents,
                           const GURL& url,
                           const content::SSLStatus& ssl);

  // PermissionSelectorViewObserver implementation.
  virtual void OnPermissionChanged(
      PermissionSelectorView* selector) OVERRIDE;

  // views::BubbleDelegate implementations.
  virtual gfx::Rect GetAnchorRect() OVERRIDE;

  // views::ButtonListener implementation.
  virtual void ButtonPressed(views::Button* button,
                             const views::Event& event) OVERRIDE;

  // views::LinkListener implementation.
  virtual void LinkClicked(views::Link* source, int event_flags) OVERRIDE;

  // views::TabbedPaneListener implementations.
  virtual void TabSelectedAt(int index) OVERRIDE;

  // views::View implementation.
  virtual gfx::Size GetPreferredSize() OVERRIDE;

  // WebsiteSettingsUI implementations.
  virtual void SetCookieInfo(const CookieInfoList& cookie_info_list) OVERRIDE;
  virtual void SetPermissionInfo(
      const PermissionInfoList& permission_info_list) OVERRIDE;
  virtual void SetIdentityInfo(const IdentityInfo& identity_info) OVERRIDE;
  virtual void SetFirstVisit(const string16& first_visit) OVERRIDE;

  // Creates the contents of the "Permissions" tab. The ownership of the
  // returned view is transferred to the caller.
  views::View* CreatePermissionsTab() WARN_UNUSED_RESULT;

  // Creates the contents of the "Identity" tab. The ownership of the returned
  // view is transferred to the caller.
  views::View* CreateIdentityTab() WARN_UNUSED_RESULT;

  // Each tab contains several sections with a |headline| followed by the
  // section |contents| and an optional |link|. This method creates a section
  // for the given |headline|, |contents| and |link|. |link| can be NULL if the
  // section should not contain a link.
  views::View* CreateSection(const string16& headline,
                             views::View* contents,
                             views::Link* link) WARN_UNUSED_RESULT;

  // Resets the |content_container| and adds the passed |icon| and |text| to
  // it. All existing child views of the |content_container| are cleared and
  // destroyed. The |icon| and |text| are added to the |content_container|
  // using a two column layout that is common to all sections on the identity
  // tab.
  void ResetContentContainer(views::View* content_container,
                             const gfx::Image& icon,
                             const string16& text);

  // The tab contents of the current tab. The popup can't live longer than a
  // tab.
  TabContents* tab_contents_;

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

  views::View* identity_info_content_;
  views::View* connection_info_content_;
  views::View* page_info_content_;

  DISALLOW_COPY_AND_ASSIGN(WebsiteSettingsPopupView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_WEBSITE_SETTINGS_WEBSITE_SETTINGS_POPUP_VIEW_H_
