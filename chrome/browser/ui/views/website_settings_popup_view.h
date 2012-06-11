// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEBSITE_SETTINGS_POPUP_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_WEBSITE_SETTINGS_POPUP_VIEW_H_
#pragma once

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "chrome/browser/ui/website_settings/website_settings_ui.h"
#include "ui/views/bubble/bubble_delegate.h"
#include "ui/views/controls/combobox/combobox_listener.h"
#include "ui/views/controls/link_listener.h"
#include "ui/views/controls/tabbed_pane/tabbed_pane_listener.h"

class GURL;
class PopupHeader;
class Profile;
class TabContents;

namespace content {
struct SSLStatus;
}

namespace views {
class Combobox;
class Link;
class Label;
class TabbedPane;
class View;
}

namespace website_settings {
class PermissionComboboxModel;
}

// The views implementation of the website settings UI.
class WebsiteSettingsPopupView : public WebsiteSettingsUI,
                                 public views::BubbleDelegateView,
                                 public views::ComboboxListener,
                                 public views::LinkListener,
                                 public views::TabbedPaneListener {
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

  // WebsiteSettingsUI implementations.
  virtual void SetCookieInfo(const CookieInfoList& cookie_info_list) OVERRIDE;
  virtual void SetPermissionInfo(
      const PermissionInfoList& permission_info_list) OVERRIDE;
  virtual void SetIdentityInfo(const IdentityInfo& identity_info) OVERRIDE;
  virtual void SetFirstVisit(const string16& first_visit) OVERRIDE;

  // views::View implementation.
  virtual gfx::Size GetPreferredSize() OVERRIDE;

  // views::BubbleDelegate implementations.
  virtual gfx::Rect GetAnchorRect() OVERRIDE;

  // views::ComboboxListener implementation.
  virtual void OnSelectedIndexChanged(views::Combobox* combobox) OVERRIDE;

  // LinkListener implementation.
  virtual void LinkClicked(views::Link* source, int event_flags) OVERRIDE;

  // views::TabbedPaneListener implementations.
  virtual void TabSelectedAt(int index) OVERRIDE;

  // Each tab contains several sections with a |headline| followed by the
  // section |contents| and an optional |link|. This method creates a section
  // for the given |headline|, |contents| and |link|. |link| can be NULL if the
  // section should not contain a link.
  views::View* CreateSection(const string16& headline,
                             views::View* contents,
                             views::Link* link) WARN_UNUSED_RESULT;

  // Creates a single row for the "Permissions" section from the "Permissions"
  // tab. Such a row contains a |permissions_label| with the name of the
  // permission and a |combobox| that allows to select setting for the given
  // permission. The ownership of the returned view is transferred to the
  // caller.
  views::View* CreatePermissionRow(
      views::Label* permission_label,
      views::Combobox* combobox) WARN_UNUSED_RESULT;

  // Creates the contents of the "Permissions" tab. The ownership of the
  // returned view is transferred to the caller.
  views::View* CreatePermissionsTab() WARN_UNUSED_RESULT;

  // Creates the contents of the "Identity" tab. The ownership of the returned
  // view is transferred to the caller.
  views::View* CreateIdentityTab() WARN_UNUSED_RESULT;

  // Creates a multiline text label that is initialized with the given |text|.
  // The ownership of the returned label is transferred to the caller.
  views::Label* CreateTextLabel(const string16& text) WARN_UNUSED_RESULT;

  // The tab contents of the current tab. The popup can't live longer than a
  // tab.
  TabContents* tab_contents_;  // Weak pointer.

  // The presenter that controlls the Website Settings UI.
  scoped_ptr<WebsiteSettings> presenter_;

  // The view |PopupHeader| is owned by the popup contents view.
  PopupHeader* header_;

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

  views::Label* identity_info_text_;
  views::Label* connection_info_text_;
  views::Label* page_info_text_;

  // A |ComboboxModel| is not owned by a |Combobox|. Therefor the popup owns
  // all |PermissionComboboxModels| and deletes them when it is destroyed.
  ScopedVector<website_settings::PermissionComboboxModel> combobox_models_;

  DISALLOW_COPY_AND_ASSIGN(WebsiteSettingsPopupView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_WEBSITE_SETTINGS_POPUP_VIEW_H_
