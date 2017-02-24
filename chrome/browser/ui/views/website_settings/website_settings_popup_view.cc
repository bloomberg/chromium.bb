// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/website_settings/website_settings_popup_view.h"

#include <stddef.h>

#include <algorithm>
#include <vector>

#include "base/i18n/rtl.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/certificate_viewer.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/collected_cookies_views.h"
#include "chrome/browser/ui/views/website_settings/chosen_object_row.h"
#include "chrome/browser/ui/views/website_settings/non_accessible_image_view.h"
#include "chrome/browser/ui/views/website_settings/permission_selector_row.h"
#include "chrome/browser/ui/website_settings/website_settings.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/strings/grit/components_chromium_strings.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/user_metrics.h"
#include "extensions/common/constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/image/image.h"
#include "ui/resources/grit/ui_resources.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_constants.h"
#include "ui/views/layout/layout_manager.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

namespace {

// NOTE(jdonnelly): This use of this process-wide variable assumes that there's
// never more than one website settings popup shown and that it's associated
// with the current window. If this assumption fails in the future, we'll need
// to return a weak pointer from ShowPopup so callers can associate it with the
// current window (or other context) and check if the popup they care about is
// showing.
WebsiteSettingsPopupView::PopupType g_shown_popup_type =
    WebsiteSettingsPopupView::POPUP_NONE;

// General constants -----------------------------------------------------------

// Popup width constraints.
const int kMinPopupWidth = 320;
const int kMaxPopupWidth = 1000;

// Security Section (PopupHeaderView) ------------------------------------------

// Margin and padding values for the |PopupHeaderView|.
const int kHeaderMarginBottom = 10;
const int kHeaderPaddingBottom = views::kPanelVertMargin;

// Spacing between labels in the header.
const int kHeaderLabelSpacing = 4;

// Site Settings Section -------------------------------------------------------

// Spacing above and below the cookies view.
const int kCookiesViewVerticalPadding = 6;

// Spacing between a permission image and the text.
const int kPermissionImageSpacing = 6;

// Spacing between rows in the site settings section
const int kPermissionsVerticalSpacing = 12;

// Button/styled label/link IDs ------------------------------------------------
const int BUTTON_CLOSE = 1337;
const int STYLED_LABEL_SECURITY_DETAILS = 1338;
const int STYLED_LABEL_RESET_CERTIFICATE_DECISIONS = 1339;
const int LINK_COOKIE_DIALOG = 1340;
const int LINK_SITE_SETTINGS = 1341;

// The default, ui::kTitleFontSizeDelta, is too large for the website settings
// bubble (e.g. +3). Use +1 to obtain a smaller font.
constexpr int kSummaryFontSizeDelta = 1;

// Adds a ColumnSet on |layout| with a single View column and padding columns
// on either side of it with |margin| width.
void AddColumnWithSideMargin(views::GridLayout* layout, int margin, int id) {
  views::ColumnSet* column_set = layout->AddColumnSet(id);
  column_set->AddPaddingColumn(0, margin);
  column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL, 1,
                        views::GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, margin);
}

}  // namespace

// |PopupHeaderView| is the UI element (view) that represents the header of the
// |WebsiteSettingsPopupView|. The header shows the status of the site's
// identity check and the name of the site's identity.
class PopupHeaderView : public views::View {
 public:
  PopupHeaderView(views::ButtonListener* button_listener,
                  views::StyledLabelListener* styled_label_listener,
                  int side_margin);
  ~PopupHeaderView() override;

  // Sets the security summary for the current page.
  void SetSummary(const base::string16& summary_text);

  // Sets the security details for the current page.
  void SetDetails(const base::string16& details_text);

  void AddResetDecisionsLabel();

 private:
  // The listener for the styled labels in this view.
  views::StyledLabelListener* styled_label_listener_;

  // The label that displays the status of the identity check for this site.
  // Includes a link to open the Chrome Help Center article about connection
  // security.
  views::StyledLabel* details_label_;

  // A container for the styled label with a link for resetting cert decisions.
  // This is only shown sometimes, so we use a container to keep track of
  // where to place it (if needed).
  views::View* reset_decisions_label_container_;
  views::StyledLabel* reset_decisions_label_;

  DISALLOW_COPY_AND_ASSIGN(PopupHeaderView);
};

// Website Settings are not supported for internal Chrome pages and extension
// pages. Instead of the |WebsiteSettingsPopupView|, the
// |InternalPageInfoPopupView| is displayed.
class InternalPageInfoPopupView : public views::BubbleDialogDelegateView {
 public:
  // If |anchor_view| is nullptr, or has no Widget, |parent_window| may be
  // provided to ensure this bubble is closed when the parent closes.
  InternalPageInfoPopupView(views::View* anchor_view,
                            gfx::NativeView parent_window,
                            const GURL& url);
  ~InternalPageInfoPopupView() override;

  // views::BubbleDialogDelegateView:
  void OnWidgetDestroying(views::Widget* widget) override;
  int GetDialogButtons() const override;

 private:
  friend class WebsiteSettingsPopupView;

  // Used around icon and inside bubble border.
  static constexpr int kSpacing = 12;

  DISALLOW_COPY_AND_ASSIGN(InternalPageInfoPopupView);
};

////////////////////////////////////////////////////////////////////////////////
// Popup Header
////////////////////////////////////////////////////////////////////////////////

PopupHeaderView::PopupHeaderView(
    views::ButtonListener* button_listener,
    views::StyledLabelListener* styled_label_listener,
    int side_margin)
    : styled_label_listener_(styled_label_listener),
      details_label_(nullptr),
      reset_decisions_label_container_(nullptr),
      reset_decisions_label_(nullptr) {
  views::GridLayout* layout = new views::GridLayout(this);
  SetLayoutManager(layout);

  const int label_column_status = 1;
  AddColumnWithSideMargin(layout, side_margin, label_column_status);
  layout->AddPaddingRow(0, kHeaderLabelSpacing);

  layout->StartRow(0, label_column_status);
  details_label_ =
      new views::StyledLabel(base::string16(), styled_label_listener);
  details_label_->set_id(STYLED_LABEL_SECURITY_DETAILS);
  layout->AddView(details_label_, 1, 1, views::GridLayout::FILL,
                  views::GridLayout::LEADING);

  layout->StartRow(0, label_column_status);
  reset_decisions_label_container_ = new views::View();
  reset_decisions_label_container_->SetLayoutManager(
      new views::BoxLayout(views::BoxLayout::kHorizontal, 0, 0, 0));
  layout->AddView(reset_decisions_label_container_, 1, 1,
                  views::GridLayout::FILL, views::GridLayout::LEADING);

  layout->AddPaddingRow(1, kHeaderPaddingBottom);
}

PopupHeaderView::~PopupHeaderView() {}

void PopupHeaderView::SetDetails(const base::string16& details_text) {
  std::vector<base::string16> subst;
  subst.push_back(details_text);
  subst.push_back(l10n_util::GetStringUTF16(IDS_LEARN_MORE));

  std::vector<size_t> offsets;

  base::string16 text = base::ReplaceStringPlaceholders(
      base::ASCIIToUTF16("$1 $2"), subst, &offsets);
  details_label_->SetText(text);
  gfx::Range details_range(offsets[1], text.length());

  views::StyledLabel::RangeStyleInfo link_style =
      views::StyledLabel::RangeStyleInfo::CreateForLink();
  if (!ui::MaterialDesignController::IsSecondaryUiMaterial())
    link_style.font_style |= gfx::Font::FontStyle::UNDERLINE;
  link_style.disable_line_wrapping = false;

  details_label_->AddStyleRange(details_range, link_style);
}

void PopupHeaderView::AddResetDecisionsLabel() {
  std::vector<base::string16> subst;
  subst.push_back(
      l10n_util::GetStringUTF16(IDS_PAGEINFO_INVALID_CERTIFICATE_DESCRIPTION));
  subst.push_back(l10n_util::GetStringUTF16(
      IDS_PAGEINFO_RESET_INVALID_CERTIFICATE_DECISIONS_BUTTON));

  std::vector<size_t> offsets;

  base::string16 text = base::ReplaceStringPlaceholders(
      base::ASCIIToUTF16("$1 $2"), subst, &offsets);
  reset_decisions_label_ = new views::StyledLabel(text, styled_label_listener_);
  reset_decisions_label_->set_id(STYLED_LABEL_RESET_CERTIFICATE_DECISIONS);
  gfx::Range link_range(offsets[1], text.length());

  views::StyledLabel::RangeStyleInfo link_style =
      views::StyledLabel::RangeStyleInfo::CreateForLink();
  if (!ui::MaterialDesignController::IsSecondaryUiMaterial())
    link_style.font_style |= gfx::Font::FontStyle::UNDERLINE;
  link_style.disable_line_wrapping = false;

  reset_decisions_label_->AddStyleRange(link_range, link_style);
  // Fit the styled label to occupy available width.
  reset_decisions_label_->SizeToFit(0);
  reset_decisions_label_container_->AddChildView(reset_decisions_label_);

  // Now that it contains a label, the container needs padding at the top.
  reset_decisions_label_container_->SetBorder(
      views::CreateEmptyBorder(8, 0, 0, 0));

  InvalidateLayout();
}

////////////////////////////////////////////////////////////////////////////////
// InternalPageInfoPopupView
////////////////////////////////////////////////////////////////////////////////

InternalPageInfoPopupView::InternalPageInfoPopupView(
    views::View* anchor_view,
    gfx::NativeView parent_window,
    const GURL& url)
    : BubbleDialogDelegateView(anchor_view, views::BubbleBorder::TOP_LEFT) {
  g_shown_popup_type = WebsiteSettingsPopupView::POPUP_INTERNAL_PAGE;
  set_parent_window(parent_window);

  int text = IDS_PAGE_INFO_INTERNAL_PAGE;
  int icon = IDR_PRODUCT_LOGO_16;
  if (url.SchemeIs(extensions::kExtensionScheme)) {
    text = IDS_PAGE_INFO_EXTENSION_PAGE;
    icon = IDR_PLUGINS_FAVICON;
  } else if (url.SchemeIs(content::kViewSourceScheme)) {
    text = IDS_PAGE_INFO_VIEW_SOURCE_PAGE;
    // view-source scheme uses the same icon as chrome:// pages.
    icon = IDR_PRODUCT_LOGO_16;
  } else if (!url.SchemeIs(content::kChromeUIScheme) &&
             !url.SchemeIs(content::kChromeDevToolsScheme)) {
    NOTREACHED();
  }

  // Compensate for built-in vertical padding in the anchor view's image.
  set_anchor_view_insets(gfx::Insets(
      GetLayoutConstant(LOCATION_BAR_BUBBLE_ANCHOR_VERTICAL_INSET), 0));

  SetLayoutManager(new views::BoxLayout(views::BoxLayout::kHorizontal, kSpacing,
                                        kSpacing, kSpacing));
  set_margins(gfx::Insets());
  views::ImageView* icon_view = new NonAccessibleImageView();
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  icon_view->SetImage(rb.GetImageSkiaNamed(icon));
  AddChildView(icon_view);

  views::Label* label = new views::Label(l10n_util::GetStringUTF16(text));
  label->SetMultiLine(true);
  label->SetAllowCharacterBreak(true);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  AddChildView(label);

  views::BubbleDialogDelegateView::CreateBubble(this);
}

InternalPageInfoPopupView::~InternalPageInfoPopupView() {
}

void InternalPageInfoPopupView::OnWidgetDestroying(views::Widget* widget) {
  g_shown_popup_type = WebsiteSettingsPopupView::POPUP_NONE;
}

int InternalPageInfoPopupView::GetDialogButtons() const {
  return ui::DIALOG_BUTTON_NONE;
}

////////////////////////////////////////////////////////////////////////////////
// WebsiteSettingsPopupView
////////////////////////////////////////////////////////////////////////////////

WebsiteSettingsPopupView::~WebsiteSettingsPopupView() {
}

// static
void WebsiteSettingsPopupView::ShowPopup(
    views::View* anchor_view,
    const gfx::Rect& anchor_rect,
    Profile* profile,
    content::WebContents* web_contents,
    const GURL& url,
    const security_state::SecurityInfo& security_info) {
  gfx::NativeView parent_window =
      anchor_view ? nullptr : web_contents->GetNativeView();
  if (url.SchemeIs(content::kChromeUIScheme) ||
      url.SchemeIs(content::kChromeDevToolsScheme) ||
      url.SchemeIs(extensions::kExtensionScheme) ||
      url.SchemeIs(content::kViewSourceScheme)) {
    // Use the concrete type so that |SetAnchorRect| can be called as a friend.
    InternalPageInfoPopupView* popup =
        new InternalPageInfoPopupView(anchor_view, parent_window, url);
    if (!anchor_view)
      popup->SetAnchorRect(anchor_rect);
    popup->GetWidget()->Show();
    return;
  }
  WebsiteSettingsPopupView* popup = new WebsiteSettingsPopupView(
      anchor_view, parent_window, profile, web_contents, url, security_info);
  if (!anchor_view)
    popup->SetAnchorRect(anchor_rect);
  popup->GetWidget()->Show();
}

// static
WebsiteSettingsPopupView::PopupType
WebsiteSettingsPopupView::GetShownPopupType() {
  return g_shown_popup_type;
}

WebsiteSettingsPopupView::WebsiteSettingsPopupView(
    views::View* anchor_view,
    gfx::NativeView parent_window,
    Profile* profile,
    content::WebContents* web_contents,
    const GURL& url,
    const security_state::SecurityInfo& security_info)
    : content::WebContentsObserver(web_contents),
      BubbleDialogDelegateView(anchor_view, views::BubbleBorder::TOP_LEFT),
      profile_(profile),
      header_(nullptr),
      separator_(nullptr),
      site_settings_view_(nullptr),
      cookies_view_(nullptr),
      cookie_dialog_link_(nullptr),
      permissions_view_(nullptr),
      weak_factory_(this) {
  g_shown_popup_type = POPUP_WEBSITE_SETTINGS;
  set_parent_window(parent_window);

  // Compensate for built-in vertical padding in the anchor view's image.
  set_anchor_view_insets(gfx::Insets(
      GetLayoutConstant(LOCATION_BAR_BUBBLE_ANCHOR_VERTICAL_INSET), 0));

  // Capture the default bubble margin, and move it to the Layout classes. This
  // is necessary so that the views::Separator can extend the full width of the
  // bubble.
  const int side_margin = margins().left();
  DCHECK_EQ(margins().left(), margins().right());

  // Also remove the top margin from the client area so there is less space
  // below the dialog title.
  set_margins(gfx::Insets(0, 0, margins().bottom(), 0));

  views::GridLayout* layout = new views::GridLayout(this);
  SetLayoutManager(layout);

  // Use a single ColumnSet here. Otherwise the preferred width doesn't properly
  // propagate up to the dialog width.
  const int content_column = 0;
  views::ColumnSet* column_set = layout->AddColumnSet(content_column);
  column_set->AddColumn(views::GridLayout::FILL,
                        views::GridLayout::FILL,
                        1,
                        views::GridLayout::USE_PREF,
                        0,
                        0);

  header_ = new PopupHeaderView(this, this, side_margin);
  layout->StartRow(1, content_column);
  layout->AddView(header_);

  layout->StartRow(0, content_column);
  separator_ = new views::Separator();
  layout->AddView(separator_);

  layout->AddPaddingRow(1, kHeaderMarginBottom);
  layout->StartRow(1, content_column);

  site_settings_view_ = CreateSiteSettingsView(side_margin);
  layout->AddView(site_settings_view_);

  if (!ui::MaterialDesignController::IsSecondaryUiMaterial()) {
    // In non-material, titles are inset from the dialog margin. Ensure the
    // horizontal insets match.
    set_title_margins(
        gfx::Insets(views::kPanelVertMargin, side_margin, 0, side_margin));
  }
  views::BubbleDialogDelegateView::CreateBubble(this);

  presenter_.reset(new WebsiteSettings(
      this, profile, TabSpecificContentSettings::FromWebContents(web_contents),
      web_contents, url, security_info));
}

void WebsiteSettingsPopupView::RenderFrameDeleted(
    content::RenderFrameHost* render_frame_host) {
  if (render_frame_host == web_contents()->GetMainFrame())
    GetWidget()->Close();
}

void WebsiteSettingsPopupView::WebContentsDestroyed() {
  weak_factory_.InvalidateWeakPtrs();
}

void WebsiteSettingsPopupView::OnPermissionChanged(
    const WebsiteSettingsUI::PermissionInfo& permission) {
  presenter_->OnSitePermissionChanged(permission.type, permission.setting);
  // The menu buttons for the permissions might have longer strings now, so we
  // need to size the whole bubble.
  SizeToContents();
}

void WebsiteSettingsPopupView::OnChosenObjectDeleted(
    const WebsiteSettingsUI::ChosenObjectInfo& info) {
  presenter_->OnSiteChosenObjectDeleted(info.ui_info, *info.object);
}

base::string16 WebsiteSettingsPopupView::GetWindowTitle() const {
  return summary_text_;
}

bool WebsiteSettingsPopupView::ShouldShowCloseButton() const {
  return true;
}

void WebsiteSettingsPopupView::OnWidgetDestroying(views::Widget* widget) {
  g_shown_popup_type = POPUP_NONE;
  presenter_->OnUIClosing();
}

int WebsiteSettingsPopupView::GetDialogButtons() const {
  return ui::DIALOG_BUTTON_NONE;
}

const gfx::FontList& WebsiteSettingsPopupView::GetTitleFontList() const {
  return ui::ResourceBundle::GetSharedInstance().GetFontListWithDelta(
      kSummaryFontSizeDelta);
}

void WebsiteSettingsPopupView::ButtonPressed(views::Button* button,
                                             const ui::Event& event) {
  DCHECK_EQ(BUTTON_CLOSE, button->id());
  GetWidget()->Close();
}

void WebsiteSettingsPopupView::LinkClicked(views::Link* source,
                                           int event_flags) {
  // The popup closes automatically when the collected cookies dialog or the
  // certificate viewer opens. So delay handling of the link clicked to avoid
  // a crash in the base class which needs to complete the mouse event handling.
  content::BrowserThread::PostTask(
      content::BrowserThread::UI, FROM_HERE,
      base::Bind(&WebsiteSettingsPopupView::HandleLinkClickedAsync,
                 weak_factory_.GetWeakPtr(), source));
}

gfx::Size WebsiteSettingsPopupView::GetPreferredSize() const {
  if (header_ == nullptr && site_settings_view_ == nullptr)
    return views::View::GetPreferredSize();

  int height = 0;
  if (header_)
    height += header_->GetPreferredSize().height() + kHeaderMarginBottom;
  if (separator_)
    height += separator_->GetPreferredSize().height();

  if (site_settings_view_)
    height += site_settings_view_->GetPreferredSize().height();

  int width = kMinPopupWidth;
  if (site_settings_view_)
    width = std::max(width, site_settings_view_->GetPreferredSize().width());
  width = std::min(width, kMaxPopupWidth);
  return gfx::Size(width, height);
}

void WebsiteSettingsPopupView::SetCookieInfo(
    const CookieInfoList& cookie_info_list) {
  // |cookie_info_list| should only ever have 2 items: first- and third-party
  // cookies.
  DCHECK_EQ(cookie_info_list.size(), 2u);
  int total_allowed = 0;
  for (const auto& i : cookie_info_list)
    total_allowed += i.allowed;
  base::string16 label_text = l10n_util::GetPluralStringFUTF16(
      IDS_WEBSITE_SETTINGS_NUM_COOKIES, total_allowed);

  if (!cookie_dialog_link_) {
    cookie_dialog_link_ = new views::Link(label_text);
    cookie_dialog_link_->set_id(LINK_COOKIE_DIALOG);
    cookie_dialog_link_->set_listener(this);
  } else {
    cookie_dialog_link_->SetText(label_text);
  }

  views::GridLayout* layout =
      static_cast<views::GridLayout*>(cookies_view_->GetLayoutManager());
  if (!layout) {
    layout = new views::GridLayout(cookies_view_);
    cookies_view_->SetLayoutManager(layout);

    const int cookies_view_column = 0;
    views::ColumnSet* column_set = layout->AddColumnSet(cookies_view_column);
    column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL, 0,
                          views::GridLayout::FIXED, kPermissionIconColumnWidth,
                          0);
    column_set->AddPaddingColumn(0, kPermissionImageSpacing);
    column_set->AddColumn(views::GridLayout::LEADING, views::GridLayout::FILL,
                          0, views::GridLayout::USE_PREF, 0, 0);

    layout->AddPaddingRow(0, kCookiesViewVerticalPadding);

    layout->StartRow(1, cookies_view_column);
    WebsiteSettingsUI::PermissionInfo info;
    info.type = CONTENT_SETTINGS_TYPE_COOKIES;
    info.setting = CONTENT_SETTING_ALLOW;
    info.is_incognito =
        Profile::FromBrowserContext(web_contents()->GetBrowserContext())
            ->IsOffTheRecord();
    views::ImageView* icon = new NonAccessibleImageView();
    const gfx::Image& image = WebsiteSettingsUI::GetPermissionIcon(info);
    icon->SetImage(image.ToImageSkia());
    layout->AddView(
        icon, 1, 2, views::GridLayout::FILL,
        // TODO: The vertical alignment may change to CENTER once Harmony is
        // implemented. See https://crbug.com/512442#c48
        views::GridLayout::LEADING);

    ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
    const gfx::FontList& font_list = rb.GetFontListWithDelta(1);
    views::Label* cookies_label = new views::Label(
        l10n_util::GetStringUTF16(IDS_WEBSITE_SETTINGS_TITLE_SITE_DATA),
        font_list);
    layout->AddView(cookies_label);
    layout->StartRow(1, cookies_view_column);
    layout->SkipColumns(1);

    layout->AddView(cookie_dialog_link_);

    layout->AddPaddingRow(0, kCookiesViewVerticalPadding);
  }

  layout->Layout(cookies_view_);
  SizeToContents();
}

void WebsiteSettingsPopupView::SetPermissionInfo(
    const PermissionInfoList& permission_info_list,
    ChosenObjectInfoList chosen_object_info_list) {
  // When a permission is changed, WebsiteSettings::OnSitePermissionChanged()
  // calls this method with updated permissions. However, PermissionSelectorRow
  // will have already updated its state, so it's already reflected in the UI.
  // In addition, if a permission is set to the default setting, WebsiteSettings
  // removes it from |permission_info_list|, but the button should remain.
  if (permissions_view_)
    return;

  permissions_view_ = new views::View();
  views::GridLayout* layout = new views::GridLayout(permissions_view_);
  permissions_view_->SetLayoutManager(layout);

  site_settings_view_->AddChildView(permissions_view_);

  const int content_column = 0;
  views::ColumnSet* column_set = layout->AddColumnSet(content_column);
  column_set->AddColumn(views::GridLayout::FILL,
                        views::GridLayout::FILL,
                        1,
                        views::GridLayout::USE_PREF,
                        0,
                        0);
  for (const auto& permission : permission_info_list) {
    layout->StartRow(1, content_column);
    PermissionSelectorRow* selector = new PermissionSelectorRow(
        profile_,
        web_contents() ? web_contents()->GetVisibleURL() : GURL::EmptyGURL(),
        permission);
    selector->AddObserver(this);
    layout->AddView(selector, 1, 1, views::GridLayout::FILL,
                    views::GridLayout::CENTER);
    layout->AddPaddingRow(1, kPermissionsVerticalSpacing);
  }

  for (auto& object : chosen_object_info_list) {
    layout->StartRow(1, content_column);
    // The view takes ownership of the object info.
    auto* object_view = new ChosenObjectRow(std::move(object));
    object_view->AddObserver(this);
    layout->AddView(object_view, 1, 1, views::GridLayout::LEADING,
                    views::GridLayout::CENTER);
    layout->AddPaddingRow(1, kPermissionsVerticalSpacing);
  }

  layout->Layout(permissions_view_);

  // Add site settings link.
  views::Link* site_settings_link = new views::Link(
      l10n_util::GetStringUTF16(IDS_PAGE_INFO_SITE_SETTINGS_LINK));
  site_settings_link->set_id(LINK_SITE_SETTINGS);
  site_settings_link->set_listener(this);
  views::View* link_section = new views::View();
  const int kLinkMarginTop = 4;
  link_section->SetLayoutManager(new views::BoxLayout(
      views::BoxLayout::kHorizontal, 0, kLinkMarginTop, 0));
  link_section->AddChildView(site_settings_link);
  site_settings_view_->AddChildView(link_section);

  SizeToContents();
}

void WebsiteSettingsPopupView::SetIdentityInfo(
    const IdentityInfo& identity_info) {
  std::unique_ptr<WebsiteSettingsUI::SecurityDescription> security_description =
      identity_info.GetSecurityDescription();

  summary_text_ = security_description->summary;
  GetWidget()->UpdateWindowTitle();

  if (identity_info.certificate) {
    certificate_ = identity_info.certificate;

    if (identity_info.show_ssl_decision_revoke_button)
      header_->AddResetDecisionsLabel();
  }

  header_->SetDetails(security_description->details);

  Layout();
  SizeToContents();
}

views::View* WebsiteSettingsPopupView::CreateSiteSettingsView(int side_margin) {
  views::View* site_settings_view = new views::View();
  views::BoxLayout* box_layout =
      new views::BoxLayout(views::BoxLayout::kVertical, side_margin, 0, 0);
  site_settings_view->SetLayoutManager(box_layout);
  box_layout->set_cross_axis_alignment(
      views::BoxLayout::CROSS_AXIS_ALIGNMENT_STRETCH);

  // Add cookies view.
  cookies_view_ = new views::View();
  site_settings_view->AddChildView(cookies_view_);

  return site_settings_view;
}

void WebsiteSettingsPopupView::HandleLinkClickedAsync(views::Link* source) {
  // Both switch cases require accessing web_contents(), so we check it here.
  if (web_contents() == nullptr || web_contents()->IsBeingDestroyed())
    return;
  switch (source->id()) {
    case LINK_SITE_SETTINGS:
      // TODO(crbug.com/655876): This opens the general Content Settings pane,
      // which is OK for now. But on Android, it opens a page specific to a
      // given origin that shows all of the settings for that origin. If/when
      // that's available on desktop we should link to that here, too.
      web_contents()->OpenURL(content::OpenURLParams(
          GURL(chrome::kChromeUIContentSettingsURL), content::Referrer(),
          WindowOpenDisposition::NEW_FOREGROUND_TAB, ui::PAGE_TRANSITION_LINK,
          false));
      presenter_->RecordWebsiteSettingsAction(
          WebsiteSettings::WEBSITE_SETTINGS_SITE_SETTINGS_OPENED);
      break;
    case LINK_COOKIE_DIALOG:
      // Count how often the Collected Cookies dialog is opened.
      presenter_->RecordWebsiteSettingsAction(
          WebsiteSettings::WEBSITE_SETTINGS_COOKIES_DIALOG_OPENED);
      new CollectedCookiesViews(web_contents());
      break;
    default:
      NOTREACHED();
  }
}

void WebsiteSettingsPopupView::StyledLabelLinkClicked(views::StyledLabel* label,
                                                      const gfx::Range& range,
                                                      int event_flags) {
  switch (label->id()) {
    case STYLED_LABEL_SECURITY_DETAILS:
      web_contents()->OpenURL(content::OpenURLParams(
          GURL(chrome::kPageInfoHelpCenterURL), content::Referrer(),
          WindowOpenDisposition::NEW_FOREGROUND_TAB, ui::PAGE_TRANSITION_LINK,
          false));
      presenter_->RecordWebsiteSettingsAction(
          WebsiteSettings::WEBSITE_SETTINGS_CONNECTION_HELP_OPENED);
      break;
    case STYLED_LABEL_RESET_CERTIFICATE_DECISIONS:
      presenter_->OnRevokeSSLErrorBypassButtonPressed();
      GetWidget()->Close();
      break;
    default:
      NOTREACHED();
  }
}
