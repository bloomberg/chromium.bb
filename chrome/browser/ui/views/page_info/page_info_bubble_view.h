// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAGE_INFO_PAGE_INFO_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PAGE_INFO_PAGE_INFO_BUBBLE_VIEW_H_

#include <memory>
#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/page_info/page_info_ui.h"
#include "chrome/browser/ui/views/bubble_anchor_util_views.h"
#include "chrome/browser/ui/views/page_info/chosen_object_view_observer.h"
#include "chrome/browser/ui/views/page_info/page_info_bubble_view_base.h"
#include "chrome/browser/ui/views/page_info/permission_selector_row.h"
#include "chrome/browser/ui/views/page_info/permission_selector_row_observer.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/link_listener.h"
#include "ui/views/controls/separator.h"
#include "ui/views/controls/styled_label_listener.h"

class Browser;
class BubbleHeaderView;
class GURL;
class HoverButton;
class Profile;

namespace content {
class WebContents;
}  // namespace content

namespace net {
class X509Certificate;
}  // namespace net

namespace security_state {
struct SecurityInfo;
}  // namespace security_state

namespace test {
class PageInfoBubbleViewTestApi;
}  // namespace test

namespace views {
class Link;
class Widget;
}  // namespace views

// The views implementation of the page info UI.
class PageInfoBubbleView : public PageInfoBubbleViewBase,
                           public PermissionSelectorRowObserver,
                           public ChosenObjectViewObserver,
                           public views::ButtonListener,
                           public views::LinkListener,
                           public views::StyledLabelListener,
                           public PageInfoUI {
 public:
  // The width of the column size for permissions and chosen object icons.
  static constexpr int kIconColumnWidth = 16;
  // The column set id of the permissions table for |permissions_view_|.
  static constexpr int kPermissionColumnSetId = 0;

  ~PageInfoBubbleView() override;

  enum PageInfoBubbleViewID {
    VIEW_ID_NONE = 0,
    VIEW_ID_PAGE_INFO_BUTTON_CLOSE,
    VIEW_ID_PAGE_INFO_BUTTON_CHANGE_PASSWORD,
    VIEW_ID_PAGE_INFO_BUTTON_WHITELIST_PASSWORD_REUSE,
    VIEW_ID_PAGE_INFO_LABEL_SECURITY_DETAILS,
    VIEW_ID_PAGE_INFO_LABEL_RESET_CERTIFICATE_DECISIONS,
    VIEW_ID_PAGE_INFO_LINK_OR_BUTTON_COOKIE_DIALOG,
    VIEW_ID_PAGE_INFO_LINK_OR_BUTTON_SITE_SETTINGS,
    VIEW_ID_PAGE_INFO_LINK_OR_BUTTON_CERTIFICATE_VIEWER,
  };

  // Creates the appropriate page info bubble for the given |url|.
  static views::BubbleDialogDelegateView* CreatePageInfoBubble(
      Browser* browser,
      content::WebContents* web_contents,
      const GURL& url,
      const security_state::SecurityInfo& security_info,
      bubble_anchor_util::Anchor);

 private:
  friend class PageInfoBubbleViewBrowserTest;
  friend class test::PageInfoBubbleViewTestApi;

  PageInfoBubbleView(views::View* anchor_view,
                     const gfx::Rect& anchor_rect,
                     gfx::NativeView parent_window,
                     Profile* profile,
                     content::WebContents* web_contents,
                     const GURL& url,
                     const security_state::SecurityInfo& security_info);

  // PageInfoBubbleViewBase:
  gfx::Size CalculatePreferredSize() const override;
  void OnWidgetDestroying(views::Widget* widget) override;
  void WebContentsDestroyed() override;

  // PermissionSelectorRowObserver:
  void OnPermissionChanged(
      const PageInfoUI::PermissionInfo& permission) override;

  // ChosenObjectViewObserver:
  void OnChosenObjectDeleted(const PageInfoUI::ChosenObjectInfo& info) override;

  // views::ButtonListener:
  void ButtonPressed(views::Button* button, const ui::Event& event) override;

  // views::LinkListener:
  void LinkClicked(views::Link* source, int event_flags) override;

  // views::StyledLabelListener:
  void StyledLabelLinkClicked(views::StyledLabel* label,
                              const gfx::Range& range,
                              int event_flags) override;

  // PageInfoUI:
  void SetCookieInfo(const CookieInfoList& cookie_info_list) override;
  void SetPermissionInfo(const PermissionInfoList& permission_info_list,
                         ChosenObjectInfoList chosen_object_info_list) override;
  void SetIdentityInfo(const IdentityInfo& identity_info) override;

  // Creates the contents of the |site_settings_view_|. The ownership of the
  // returned view is transferred to the caller.
  views::View* CreateSiteSettingsView() WARN_UNUSED_RESULT;

  // Posts a task to HandleMoreInfoRequestAsync() below.
  void HandleMoreInfoRequest(views::View* source);

  // Used to asynchronously handle clicks since these calls may cause the
  // destruction of the settings view and the base class window still needs to
  // be alive to finish handling the mouse or keyboard click.
  void HandleMoreInfoRequestAsync(int view_id);

  // The presenter that controls the Page Info UI.
  std::unique_ptr<PageInfo> presenter_;

  Profile* profile_;

  // The header section (containing security-related information).
  BubbleHeaderView* header_;

  // The view that contains the certificate, cookie, and permissions sections.
  views::View* site_settings_view_;

  // The link that opens the "Cookies" dialog. Non-harmony mode only.
  views::Link* cookie_link_legacy_;
  // The bubble that opens the "Cookies" dialog. Harmony mode only.
  HoverButton* cookie_button_;

  // The view that contains the "Permissions" table of the bubble.
  views::View* permissions_view_;

  // The certificate provided by the site, if one exists.
  scoped_refptr<net::X509Certificate> certificate_;

  // These rows bundle together all the |View|s involved in a single row of the
  // permissions section, and keep those views updated when the underlying
  // |Permission| changes.
  std::vector<std::unique_ptr<PermissionSelectorRow>> selector_rows_;

  base::WeakPtrFactory<PageInfoBubbleView> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(PageInfoBubbleView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_PAGE_INFO_PAGE_INFO_BUBBLE_VIEW_H_
