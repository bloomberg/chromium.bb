// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEBSITE_SETTINGS_WEBSITE_SETTINGS_POPUP_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_WEBSITE_SETTINGS_WEBSITE_SETTINGS_POPUP_VIEW_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/views/website_settings/chosen_object_row_observer.h"
#include "chrome/browser/ui/views/website_settings/permission_selector_row_observer.h"
#include "chrome/browser/ui/website_settings/website_settings_ui.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/views/bubble/bubble_dialog_delegate.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/link_listener.h"
#include "ui/views/controls/separator.h"
#include "ui/views/controls/styled_label_listener.h"

class GURL;
class PopupHeaderView;
class Profile;

namespace content {
class WebContents;
}

namespace net {
class X509Certificate;
}

namespace security_state {
struct SecurityInfo;
}  // namespace security_state

namespace test {
class WebsiteSettingsPopupViewTestApi;
}

namespace views {
class Link;
class Widget;
}

enum : int {
  // Left icon margin.
  kPermissionIconMarginLeft = 6,
  // The width of the column that contains the permissions icons.
  kPermissionIconColumnWidth = 16,
};

// The views implementation of the website settings UI.
class WebsiteSettingsPopupView : public content::WebContentsObserver,
                                 public PermissionSelectorRowObserver,
                                 public ChosenObjectRowObserver,
                                 public views::BubbleDialogDelegateView,
                                 public views::ButtonListener,
                                 public views::LinkListener,
                                 public views::StyledLabelListener,
                                 public WebsiteSettingsUI {
 public:
  ~WebsiteSettingsPopupView() override;

  // Type of the popup being displayed.
  enum PopupType {
    POPUP_NONE,
    // Usual page info bubble for websites.
    POPUP_WEBSITE_SETTINGS,
    // Custom bubble for internal pages like chrome:// and chrome-extensions://.
    POPUP_INTERNAL_PAGE
  };

  // If |anchor_view| is null, |anchor_rect| is used to anchor the bubble.
  static void ShowPopup(views::View* anchor_view,
                        const gfx::Rect& anchor_rect,
                        Profile* profile,
                        content::WebContents* web_contents,
                        const GURL& url,
                        const security_state::SecurityInfo& security_info);

  // Returns the type of the popup bubble being shown.
  static PopupType GetShownPopupType();

 private:
  friend class test::WebsiteSettingsPopupViewTestApi;

  WebsiteSettingsPopupView(views::View* anchor_view,
                           gfx::NativeView parent_window,
                           Profile* profile,
                           content::WebContents* web_contents,
                           const GURL& url,
                           const security_state::SecurityInfo& security_info);

  // WebContentsObserver implementation.
  void RenderFrameDeleted(content::RenderFrameHost* render_frame_host) override;
  void WebContentsDestroyed() override;

  // PermissionSelectorRowObserver implementation.
  void OnPermissionChanged(
      const WebsiteSettingsUI::PermissionInfo& permission) override;

  // ChosenObjectRowObserver implementation.
  void OnChosenObjectDeleted(
      const WebsiteSettingsUI::ChosenObjectInfo& info) override;

  // views::BubbleDialogDelegateView implementation.
  void OnWidgetDestroying(views::Widget* widget) override;
  int GetDialogButtons() const override;

  // views::ButtonListener implementation.
  void ButtonPressed(views::Button* button, const ui::Event& event) override;

  // views::LinkListener implementation.
  void LinkClicked(views::Link* source, int event_flags) override;

  // views::StyledLabelListener implementation.
  void StyledLabelLinkClicked(views::StyledLabel* label,
                              const gfx::Range& range,
                              int event_flags) override;

  // views::View implementation.
  gfx::Size GetPreferredSize() const override;

  // WebsiteSettingsUI implementations.
  void SetCookieInfo(const CookieInfoList& cookie_info_list) override;
  void SetPermissionInfo(const PermissionInfoList& permission_info_list,
                         ChosenObjectInfoList chosen_object_info_list) override;
  void SetIdentityInfo(const IdentityInfo& identity_info) override;
  // TODO(lgarron): Remove SetSelectedTab() with https://crbug.com/571533
  void SetSelectedTab(TabId tab_id) override;

  // Creates the contents of the |site_settings_view_|. The ownership of the
  // returned view is transferred to the caller.
  views::View* CreateSiteSettingsView() WARN_UNUSED_RESULT;

  // Used to asynchronously handle clicks since these calls may cause the
  // destruction of the settings view and the base class window still needs to
  // be alive to finish handling the mouse or keyboard click.
  void HandleLinkClickedAsync(views::Link* source);

  // Whether DevTools is disabled for the relevant profile.
  bool is_devtools_disabled_;

  // The presenter that controls the Website Settings UI.
  std::unique_ptr<WebsiteSettings> presenter_;

  Profile* profile_;

  // The header section (containing security-related information).
  PopupHeaderView* header_;

  // The separator between the header and the site settings view.
  views::Separator* separator_;

  // The view that contains the cookie and permissions sections.
  views::View* site_settings_view_;
  // The view that contains the contents of the "Cookies" part of the site
  // settings view.
  views::View* cookies_view_;
  // The link that opens the "Cookies" dialog.
  views::Link* cookie_dialog_link_;
  // The view that contains the "Permissions" table of the site settings view.
  views::View* permissions_view_;

  // The certificate provided by the site, if one exists.
  scoped_refptr<net::X509Certificate> certificate_;

  base::WeakPtrFactory<WebsiteSettingsPopupView> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(WebsiteSettingsPopupView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_WEBSITE_SETTINGS_WEBSITE_SETTINGS_POPUP_VIEW_H_
