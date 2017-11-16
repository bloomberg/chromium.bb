// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_info/page_info_bubble_view.h"

#include <stddef.h>

#include <algorithm>
#include <utility>

#include "base/i18n/rtl.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/certificate_viewer.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/page_info/page_info.h"
#include "chrome/browser/ui/page_info/page_info_dialog.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/bubble_anchor_util_views.h"
#include "chrome/browser/ui/views/collected_cookies_views.h"
#include "chrome/browser/ui/views/harmony/chrome_layout_provider.h"
#include "chrome/browser/ui/views/harmony/chrome_typography.h"
#include "chrome/browser/ui/views/page_info/chosen_object_row.h"
#include "chrome/browser/ui/views/page_info/non_accessible_image_view.h"
#include "chrome/browser/ui/views/page_info/permission_selector_row.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/theme_resources.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/strings/grit/components_chromium_strings.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/common/constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_features.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/image/image.h"
#include "ui/views/border.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/combobox/combobox.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_manager.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"
#include "url/gurl.h"

#if !defined(OS_MACOSX) || BUILDFLAG(MAC_VIEWS_BROWSER)
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/location_bar/location_icon_view.h"
#endif

using bubble_anchor_util::GetPageInfoAnchorRect;
using bubble_anchor_util::GetPageInfoAnchorView;
using views::GridLayout;

namespace {

// NOTE(jdonnelly): The following two process-wide variables assume that there's
// never more than one page info bubble shown and that it's associated with the
// current window. If this assumption fails in the future, we'll need to return
// a weak pointer from ShowBubble so callers can associate it with the current
// window (or other context) and check if the bubble they care about is showing.
PageInfoBubbleView::BubbleType g_shown_bubble_type =
    PageInfoBubbleView::BUBBLE_NONE;
views::BubbleDialogDelegateView* g_page_info_bubble = nullptr;

// General constants -----------------------------------------------------------

// Bubble width constraints.
constexpr int kMinBubbleWidth = 320;
constexpr int kMaxBubbleWidth = 1000;

// Adds a ColumnSet on |layout| with a single View column and padding columns
// on either side of it with |margin| width.
void AddColumnWithSideMargin(GridLayout* layout, int margin, int id) {
  views::ColumnSet* column_set = layout->AddColumnSet(id);
  column_set->AddPaddingColumn(0, margin);
  column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, 1,
                        GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, margin);
}

// Creates a section containing a title, icon, and link. Used to display
// Cookies and Certificate information.
// *----------------------------------------------*
// | Icon | Title (Cookies/Certificate)           |
// |----------------------------------------------|
// |      | Link                                  |
// *----------------------------------------------*
views::View* CreateInspectLinkSection(const gfx::ImageSkia& image_icon,
                                      const int title_id,
                                      views::Link* link) {
  views::View* new_view = new views::View();

  GridLayout* layout = GridLayout::CreateAndInstall(new_view);

  const int column = 0;
  views::ColumnSet* column_set = layout->AddColumnSet(column);
  column_set->AddColumn(GridLayout::CENTER, GridLayout::CENTER, 0,
                        GridLayout::FIXED, PageInfoBubbleView::kIconColumnWidth,
                        0);
  column_set->AddPaddingColumn(0,
                               ChromeLayoutProvider::Get()->GetDistanceMetric(
                                   views::DISTANCE_RELATED_LABEL_HORIZONTAL));
  column_set->AddColumn(GridLayout::LEADING, GridLayout::FILL, 0,
                        GridLayout::USE_PREF, 0, 0);

  layout->StartRow(1, column);
  views::ImageView* icon = new NonAccessibleImageView();
  icon->SetImage(image_icon);
  layout->AddView(icon);

  views::Label* title_label = new views::Label(
      l10n_util::GetStringUTF16(title_id), CONTEXT_BODY_TEXT_LARGE);
  layout->AddView(title_label);

  layout->StartRow(1, column);
  layout->SkipColumns(1);
  layout->AddView(link);
  return new_view;
}

views::View* CreateSiteSettingsLink(const int side_margin,
                                    views::LinkListener* listener) {
  views::Link* site_settings_link = new views::Link(
      l10n_util::GetStringUTF16(IDS_PAGE_INFO_SITE_SETTINGS_LINK));
  site_settings_link->set_id(
      PageInfoBubbleView::VIEW_ID_PAGE_INFO_LINK_SITE_SETTINGS);
  site_settings_link->SetTooltipText(
      l10n_util::GetStringUTF16(IDS_PAGE_INFO_SITE_SETTINGS_TOOLTIP));
  site_settings_link->set_listener(listener);
  site_settings_link->SetUnderline(false);
  views::View* link_section = new views::View();
  link_section->SetLayoutManager(new views::BoxLayout(
      views::BoxLayout::kHorizontal, gfx::Insets(0, side_margin)));
  link_section->AddChildView(site_settings_link);
  return link_section;
}

}  // namespace

// |BubbleHeaderView| is the UI element (view) that represents the header of the
// |PageInfoBubbleView|. The header shows the status of the site's
// identity check and the name of the site's identity.
class BubbleHeaderView : public views::View {
 public:
  BubbleHeaderView(views::ButtonListener* button_listener,
                   views::StyledLabelListener* styled_label_listener,
                   int side_margin);
  ~BubbleHeaderView() override;

  // Sets the security summary for the current page.
  void SetSummary(const base::string16& summary_text);

  // Sets the security details for the current page.
  void SetDetails(const base::string16& details_text);

  void AddResetDecisionsLabel();

  void AddPasswordReuseButtons();

 private:
  // The listener for the buttons in this view.
  views::ButtonListener* button_listener_;

  // The listener for the styled labels in this view.
  views::StyledLabelListener* styled_label_listener_;

  // The label that displays the status of the identity check for this site.
  // Includes a link to open the Chrome Help Center article about connection
  // security.
  views::StyledLabel* security_details_label_;

  // A container for the styled label with a link for resetting cert decisions.
  // This is only shown sometimes, so we use a container to keep track of
  // where to place it (if needed).
  views::View* reset_decisions_label_container_;
  views::StyledLabel* reset_cert_decisions_label_;

  // A container for the label buttons used to change password or mark the site
  // as safe.
  views::View* password_reuse_button_container_;
  views::LabelButton* change_password_button_;
  views::LabelButton* whitelist_password_reuse_button_;

  DISALLOW_COPY_AND_ASSIGN(BubbleHeaderView);
};

// The regular PageInfoBubbleView is not supported for internal Chrome pages and
// extension pages. Instead of the |PageInfoBubbleView|, the
// |InternalPageInfoBubbleView| is displayed.
class InternalPageInfoBubbleView : public views::BubbleDialogDelegateView {
 public:
  // If |anchor_view| is nullptr, or has no Widget, |parent_window| may be
  // provided to ensure this bubble is closed when the parent closes.
  InternalPageInfoBubbleView(views::View* anchor_view,
                             const gfx::Rect& anchor_rect,
                             gfx::NativeView parent_window,
                             const GURL& url);
  ~InternalPageInfoBubbleView() override;

  // views::BubbleDialogDelegateView:
  int GetDialogButtons() const override;
  base::string16 GetWindowTitle() const override;
  bool ShouldShowCloseButton() const override;
  gfx::ImageSkia GetWindowIcon() override;
  bool ShouldShowWindowIcon() const override;
  void OnWidgetDestroying(views::Widget* widget) override;

 private:
  base::string16 title_text_;
  gfx::ImageSkia* bubble_icon_;

  DISALLOW_COPY_AND_ASSIGN(InternalPageInfoBubbleView);
};

////////////////////////////////////////////////////////////////////////////////
// Bubble Header
////////////////////////////////////////////////////////////////////////////////

BubbleHeaderView::BubbleHeaderView(
    views::ButtonListener* button_listener,
    views::StyledLabelListener* styled_label_listener,
    int side_margin)
    : button_listener_(button_listener),
      styled_label_listener_(styled_label_listener),
      security_details_label_(nullptr),
      reset_decisions_label_container_(nullptr),
      reset_cert_decisions_label_(nullptr),
      password_reuse_button_container_(nullptr),
      change_password_button_(nullptr),
      whitelist_password_reuse_button_(nullptr) {
  GridLayout* layout = GridLayout::CreateAndInstall(this);

  const int label_column_status = 1;
  AddColumnWithSideMargin(layout, side_margin, label_column_status);

  layout->StartRow(0, label_column_status);

  security_details_label_ =
      new views::StyledLabel(base::string16(), styled_label_listener);
  security_details_label_->set_id(
      PageInfoBubbleView::VIEW_ID_PAGE_INFO_LABEL_SECURITY_DETAILS);
  layout->AddView(security_details_label_, 1, 1, GridLayout::FILL,
                  GridLayout::LEADING);

  layout->StartRow(0, label_column_status);
  reset_decisions_label_container_ = new views::View();
  reset_decisions_label_container_->SetLayoutManager(
      new views::BoxLayout(views::BoxLayout::kHorizontal));
  layout->AddView(reset_decisions_label_container_, 1, 1, GridLayout::FILL,
                  GridLayout::LEADING);

  layout->StartRow(0, label_column_status);
  password_reuse_button_container_ = new views::View();
  layout->AddView(password_reuse_button_container_, 1, 1, GridLayout::FILL,
                  GridLayout::LEADING);
}

BubbleHeaderView::~BubbleHeaderView() {}

void BubbleHeaderView::SetDetails(const base::string16& details_text) {
  std::vector<base::string16> subst;
  subst.push_back(details_text);
  subst.push_back(l10n_util::GetStringUTF16(IDS_LEARN_MORE));

  std::vector<size_t> offsets;

  base::string16 text = base::ReplaceStringPlaceholders(
      base::ASCIIToUTF16("$1 $2"), subst, &offsets);
  security_details_label_->SetText(text);
  gfx::Range details_range(offsets[1], text.length());

  views::StyledLabel::RangeStyleInfo link_style =
      views::StyledLabel::RangeStyleInfo::CreateForLink();
  link_style.disable_line_wrapping = false;

  security_details_label_->AddStyleRange(details_range, link_style);
}

void BubbleHeaderView::AddResetDecisionsLabel() {
  std::vector<base::string16> subst;
  subst.push_back(
      l10n_util::GetStringUTF16(IDS_PAGE_INFO_INVALID_CERTIFICATE_DESCRIPTION));
  subst.push_back(l10n_util::GetStringUTF16(
      IDS_PAGE_INFO_RESET_INVALID_CERTIFICATE_DECISIONS_BUTTON));

  std::vector<size_t> offsets;

  base::string16 text = base::ReplaceStringPlaceholders(
      base::ASCIIToUTF16("$1 $2"), subst, &offsets);
  reset_cert_decisions_label_ =
      new views::StyledLabel(text, styled_label_listener_);
  reset_cert_decisions_label_->set_id(
      PageInfoBubbleView::VIEW_ID_PAGE_INFO_LABEL_RESET_CERTIFICATE_DECISIONS);
  gfx::Range link_range(offsets[1], text.length());

  views::StyledLabel::RangeStyleInfo link_style =
      views::StyledLabel::RangeStyleInfo::CreateForLink();
  link_style.disable_line_wrapping = false;

  reset_cert_decisions_label_->AddStyleRange(link_range, link_style);
  // Fit the styled label to occupy available width.
  reset_cert_decisions_label_->SizeToFit(0);
  reset_decisions_label_container_->AddChildView(reset_cert_decisions_label_);

  // Now that it contains a label, the container needs padding at the top.
  reset_decisions_label_container_->SetBorder(
      views::CreateEmptyBorder(8, 0, 0, 0));

  InvalidateLayout();
}

void BubbleHeaderView::AddPasswordReuseButtons() {
  change_password_button_ = views::MdTextButton::CreateSecondaryUiBlueButton(
      button_listener_,
      l10n_util::GetStringUTF16(IDS_PAGE_INFO_CHANGE_PASSWORD_BUTTON));
  change_password_button_->set_id(
      PageInfoBubbleView::VIEW_ID_PAGE_INFO_BUTTON_CHANGE_PASSWORD);
  whitelist_password_reuse_button_ =
      views::MdTextButton::CreateSecondaryUiButton(
          button_listener_, l10n_util::GetStringUTF16(
                                IDS_PAGE_INFO_WHITELIST_PASSWORD_REUSE_BUTTON));
  whitelist_password_reuse_button_->set_id(
      PageInfoBubbleView::VIEW_ID_PAGE_INFO_BUTTON_WHITELIST_PASSWORD_REUSE);

  int kSpacingBetweenButtons = 8;

  // If these two buttons cannot fit into a single line, stack them vertically.
  bool can_fit_in_one_line =
      (password_reuse_button_container_->width() - kSpacingBetweenButtons) >=
      (change_password_button_->CalculatePreferredSize().width() +
       whitelist_password_reuse_button_->CalculatePreferredSize().width());
  views::BoxLayout* layout =
      new views::BoxLayout(can_fit_in_one_line ? views::BoxLayout::kHorizontal
                                               : views::BoxLayout::kVertical,
                           gfx::Insets(), kSpacingBetweenButtons);
  // Make buttons left-aligned. For RTL languages, buttons will automatically
  // become right-aligned.
  layout->set_main_axis_alignment(views::BoxLayout::MAIN_AXIS_ALIGNMENT_START);
  password_reuse_button_container_->SetLayoutManager(layout);

#if defined(OS_WIN) || defined(OS_CHROMEOS)
  password_reuse_button_container_->AddChildView(change_password_button_);
  password_reuse_button_container_->AddChildView(
      whitelist_password_reuse_button_);
#else
  password_reuse_button_container_->AddChildView(
      whitelist_password_reuse_button_);
  password_reuse_button_container_->AddChildView(change_password_button_);
#endif

  // Add padding at the top.
  password_reuse_button_container_->SetBorder(
      views::CreateEmptyBorder(8, 0, 0, 0));

  InvalidateLayout();
}

////////////////////////////////////////////////////////////////////////////////
// InternalPageInfoBubbleView
////////////////////////////////////////////////////////////////////////////////

InternalPageInfoBubbleView::InternalPageInfoBubbleView(
    views::View* anchor_view,
    const gfx::Rect& anchor_rect,
    gfx::NativeView parent_window,
    const GURL& url)
    : BubbleDialogDelegateView(anchor_view, views::BubbleBorder::TOP_LEFT) {
  g_shown_bubble_type = PageInfoBubbleView::BUBBLE_INTERNAL_PAGE;
  g_page_info_bubble = this;
  set_parent_window(parent_window);
  if (!anchor_view)
    SetAnchorRect(anchor_rect);

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

  // Title insets assume there is content (and thus have no bottom padding). Use
  // dialog insets to get the bottom margin back.
  set_title_margins(
      ChromeLayoutProvider::Get()->GetInsetsMetric(views::INSETS_DIALOG));
  set_margins(gfx::Insets());

  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  bubble_icon_ = rb.GetImageSkiaNamed(icon);
  title_text_ = l10n_util::GetStringUTF16(text);

  views::BubbleDialogDelegateView::CreateBubble(this);

  // Use a normal label's style for the title since there is no content.
  views::Label* title_label =
      static_cast<views::Label*>(GetBubbleFrameView()->title());
  title_label->SetFontList(views::Label::GetDefaultFontList());
  title_label->SetMultiLine(false);
  title_label->SetElideBehavior(gfx::NO_ELIDE);

  SizeToContents();
}

InternalPageInfoBubbleView::~InternalPageInfoBubbleView() {}

int InternalPageInfoBubbleView::GetDialogButtons() const {
  return ui::DIALOG_BUTTON_NONE;
}

base::string16 InternalPageInfoBubbleView::GetWindowTitle() const {
  return title_text_;
}

bool InternalPageInfoBubbleView::ShouldShowCloseButton() const {
  // TODO(patricialor): When Harmony is default, also remove |bubble_icon_| and
  // supporting code.
  return ui::MaterialDesignController::IsSecondaryUiMaterial();
}

gfx::ImageSkia InternalPageInfoBubbleView::GetWindowIcon() {
  return *bubble_icon_;
}

bool InternalPageInfoBubbleView::ShouldShowWindowIcon() const {
  return ChromeLayoutProvider::Get()->ShouldShowWindowIcon();
}

void InternalPageInfoBubbleView::OnWidgetDestroying(views::Widget* widget) {
  g_shown_bubble_type = PageInfoBubbleView::BUBBLE_NONE;
  g_page_info_bubble = nullptr;
}

////////////////////////////////////////////////////////////////////////////////
// PageInfoBubbleView
////////////////////////////////////////////////////////////////////////////////

PageInfoBubbleView::~PageInfoBubbleView() {}

// static
views::BubbleDialogDelegateView* PageInfoBubbleView::CreatePageInfoBubble(
    Browser* browser,
    content::WebContents* web_contents,
    const GURL& url,
    const security_state::SecurityInfo& security_info) {
  views::View* anchor_view = GetPageInfoAnchorView(browser);
  gfx::Rect anchor_rect =
      anchor_view ? gfx::Rect() : GetPageInfoAnchorRect(browser);
  gfx::NativeView parent_window =
      platform_util::GetViewForWindow(browser->window()->GetNativeWindow());

  if (url.SchemeIs(content::kChromeUIScheme) ||
      url.SchemeIs(content::kChromeDevToolsScheme) ||
      url.SchemeIs(extensions::kExtensionScheme) ||
      url.SchemeIs(content::kViewSourceScheme)) {
    return new InternalPageInfoBubbleView(anchor_view, anchor_rect,
                                          parent_window, url);
  }

  return new PageInfoBubbleView(anchor_view, anchor_rect, parent_window,
                                browser->profile(), web_contents, url,
                                security_info);
}

// static
PageInfoBubbleView::BubbleType PageInfoBubbleView::GetShownBubbleType() {
  return g_shown_bubble_type;
}

// static
views::BubbleDialogDelegateView* PageInfoBubbleView::GetPageInfoBubble() {
  return g_page_info_bubble;
}

PageInfoBubbleView::PageInfoBubbleView(
    views::View* anchor_view,
    const gfx::Rect& anchor_rect,
    gfx::NativeView parent_window,
    Profile* profile,
    content::WebContents* web_contents,
    const GURL& url,
    const security_state::SecurityInfo& security_info)
    : content::WebContentsObserver(web_contents),
      BubbleDialogDelegateView(anchor_view, views::BubbleBorder::TOP_LEFT),
      profile_(profile),
      header_(nullptr),
      site_settings_view_(nullptr),
      cookie_dialog_link_(nullptr),
      weak_factory_(this) {
  g_shown_bubble_type = BUBBLE_PAGE_INFO;
  g_page_info_bubble = this;
  set_parent_window(parent_window);
  if (!anchor_view)
    SetAnchorRect(anchor_rect);

  // Compensate for built-in vertical padding in the anchor view's image.
  set_anchor_view_insets(gfx::Insets(
      GetLayoutConstant(LOCATION_BAR_BUBBLE_ANCHOR_VERTICAL_INSET), 0));

  // Capture the default bubble margin, and move it to the Layout classes. This
  // is necessary so that the views::Separator can extend the full width of the
  // bubble.
  const int side_margin = margins().left();
  DCHECK_EQ(margins().left(), margins().right());
  set_margins(gfx::Insets(margins().top(), 0, margins().bottom(), 0));

  GridLayout* layout = GridLayout::CreateAndInstall(this);
  constexpr int kColumnId = 0;
  views::ColumnSet* column_set = layout->AddColumnSet(kColumnId);
  column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, 1,
                        GridLayout::USE_PREF, 0, 0);

  layout->StartRow(0, kColumnId);
  header_ = new BubbleHeaderView(this, this, side_margin);
  layout->AddView(header_);

  layout->StartRow(0, kColumnId);
  permissions_view_ = new views::View;
  layout->AddView(permissions_view_);

  layout->StartRow(0, kColumnId);
  layout->AddView(new views::Separator());

  ChromeLayoutProvider* layout_provider = ChromeLayoutProvider::Get();
  const int vertical_spacing = layout_provider->GetDistanceMetric(
      views::DISTANCE_UNRELATED_CONTROL_VERTICAL);
  layout->StartRowWithPadding(0, kColumnId, 0, vertical_spacing);
  site_settings_view_ = CreateSiteSettingsView(side_margin);
  layout->AddView(site_settings_view_);

  layout->StartRowWithPadding(0, kColumnId, 0, vertical_spacing);
  layout->AddView(CreateSiteSettingsLink(side_margin, this));

  if (!ui::MaterialDesignController::IsSecondaryUiMaterial()) {
    // In non-material, titles are inset from the dialog margin. Ensure the
    // horizontal insets match.
    set_title_margins(gfx::Insets(
        layout_provider->GetInsetsMetric(views::INSETS_DIALOG).top(),
        side_margin, 0, side_margin));
  }
  views::BubbleDialogDelegateView::CreateBubble(this);

  presenter_.reset(new PageInfo(
      this, profile, TabSpecificContentSettings::FromWebContents(web_contents),
      web_contents, url, security_info));
}

void PageInfoBubbleView::RenderFrameDeleted(
    content::RenderFrameHost* render_frame_host) {
  if (render_frame_host == web_contents()->GetMainFrame()) {
    GetWidget()->Close();
  }
}

void PageInfoBubbleView::WebContentsDestroyed() {
  weak_factory_.InvalidateWeakPtrs();
}

void PageInfoBubbleView::WasHidden() {
  GetWidget()->Close();
}

void PageInfoBubbleView::DidStartNavigation(content::NavigationHandle* handle) {
  GetWidget()->Close();
}

void PageInfoBubbleView::OnPermissionChanged(
    const PageInfoUI::PermissionInfo& permission) {
  presenter_->OnSitePermissionChanged(permission.type, permission.setting);
  // The menu buttons for the permissions might have longer strings now, so we
  // need to layout and size the whole bubble.
  Layout();
  SizeToContents();
}

void PageInfoBubbleView::OnChosenObjectDeleted(
    const PageInfoUI::ChosenObjectInfo& info) {
  presenter_->OnSiteChosenObjectDeleted(info.ui_info, *info.object);
}

base::string16 PageInfoBubbleView::GetWindowTitle() const {
  return summary_text_;
}

bool PageInfoBubbleView::ShouldShowCloseButton() const {
  return true;
}

void PageInfoBubbleView::OnWidgetDestroying(views::Widget* widget) {
  g_shown_bubble_type = BUBBLE_NONE;
  g_page_info_bubble = nullptr;
  presenter_->OnUIClosing();
}

int PageInfoBubbleView::GetDialogButtons() const {
  return ui::DIALOG_BUTTON_NONE;
}

void PageInfoBubbleView::ButtonPressed(views::Button* button,
                                       const ui::Event& event) {
  switch (button->id()) {
    case PageInfoBubbleView::VIEW_ID_PAGE_INFO_BUTTON_CLOSE:
      GetWidget()->Close();
      break;
    case PageInfoBubbleView::VIEW_ID_PAGE_INFO_BUTTON_CHANGE_PASSWORD:
      presenter_->OnChangePasswordButtonPressed(web_contents());
      break;
    case PageInfoBubbleView::VIEW_ID_PAGE_INFO_BUTTON_WHITELIST_PASSWORD_REUSE:
      GetWidget()->Close();
      presenter_->OnWhitelistPasswordReuseButtonPressed(web_contents());
      break;
    default:
      NOTREACHED();
  }
}

void PageInfoBubbleView::LinkClicked(views::Link* source, int event_flags) {
  // The bubble closes automatically when the collected cookies dialog or the
  // certificate viewer opens. So delay handling of the link clicked to avoid
  // a crash in the base class which needs to complete the mouse event handling.
  content::BrowserThread::PostTask(
      content::BrowserThread::UI, FROM_HERE,
      base::BindOnce(&PageInfoBubbleView::HandleLinkClickedAsync,
                     weak_factory_.GetWeakPtr(), source));
}

gfx::Size PageInfoBubbleView::CalculatePreferredSize() const {
  if (header_ == nullptr && site_settings_view_ == nullptr) {
    return views::View::CalculatePreferredSize();
  }

  int height = views::View::CalculatePreferredSize().height();
  int width = kMinBubbleWidth;
  // Don't get any smaller than the current size.
  width = std::max(width, GetLocalBounds().width());
  if (site_settings_view_) {
    width = std::max(width, permissions_view_->GetPreferredSize().width());
  }
  width = std::min(width, kMaxBubbleWidth);
  return gfx::Size(width, height);
}

void PageInfoBubbleView::SetCookieInfo(const CookieInfoList& cookie_info_list) {
  // This method gets called each time site data is updated, so if the cookie
  // link already exists, replace the text instead of creating a new one.
  if (cookie_dialog_link_ == nullptr) {
    // Create the link for the Cookies section.
    cookie_dialog_link_ = new views::Link(
        l10n_util::GetPluralStringFUTF16(IDS_PAGE_INFO_NUM_COOKIES, 0));
    cookie_dialog_link_->set_id(
        PageInfoBubbleView::VIEW_ID_PAGE_INFO_LINK_COOKIE_DIALOG);
    cookie_dialog_link_->set_listener(this);
    cookie_dialog_link_->SetUnderline(false);

    // Get the icon.
    PageInfoUI::PermissionInfo info;
    info.type = CONTENT_SETTINGS_TYPE_COOKIES;
    info.setting = CONTENT_SETTING_ALLOW;
    info.is_incognito =
        Profile::FromBrowserContext(web_contents()->GetBrowserContext())
            ->IsOffTheRecord();
    const gfx::ImageSkia icon =
        PageInfoUI::GetPermissionIcon(info).AsImageSkia();

    site_settings_view_->AddChildView(CreateInspectLinkSection(
        icon, IDS_PAGE_INFO_COOKIES, cookie_dialog_link_));
  }

  // Calculate the number of cookies used by this site. |cookie_info_list|
  // should only ever have 2 items: first- and third-party cookies.
  DCHECK_EQ(cookie_info_list.size(), 2u);
  int total_allowed = 0;
  for (const auto& i : cookie_info_list) {
    total_allowed += i.allowed;
  }
  base::string16 label_text = l10n_util::GetPluralStringFUTF16(
      IDS_PAGE_INFO_NUM_COOKIES, total_allowed);
  cookie_dialog_link_->SetText(label_text);
  cookie_dialog_link_->SetTooltipText(
      l10n_util::GetStringUTF16(IDS_PAGE_INFO_COOKIES_TOOLTIP));

  Layout();
  SizeToContents();
}

void PageInfoBubbleView::SetPermissionInfo(
    const PermissionInfoList& permission_info_list,
    ChosenObjectInfoList chosen_object_info_list) {
  // When a permission is changed, PageInfo::OnSitePermissionChanged()
  // calls this method with updated permissions. However, PermissionSelectorRow
  // will have already updated its state, so it's already reflected in the UI.
  // In addition, if a permission is set to the default setting, PageInfo
  // removes it from |permission_info_list|, but the button should remain.
  if (permissions_view_->has_children())
    return;

  GridLayout* layout = GridLayout::CreateAndInstall(permissions_view_);

  ChromeLayoutProvider* layout_provider = ChromeLayoutProvider::Get();
  const int list_item_padding =
      layout_provider->GetDistanceMetric(DISTANCE_CONTROL_LIST_VERTICAL);
  if (!permission_info_list.empty() || !chosen_object_info_list.empty()) {
    layout->AddPaddingRow(0, list_item_padding);
  } else {
    // If nothing to show, just add padding above the separator and exit.
    layout->AddPaddingRow(0, layout_provider->GetDistanceMetric(
                                 views::DISTANCE_UNRELATED_CONTROL_VERTICAL));
    return;
  }

  constexpr float kFixed = 0.f;
  constexpr float kStretchy = 1.f;
  int side_margin =
      layout_provider->GetInsetsMetric(views::INSETS_DIALOG).left();
  // A permissions row will have an icon, title, and combobox, with a padding
  // column on either side to match the dialog insets. Note the combobox can be
  // variable widths depending on the text inside, so allow that column to
  // expand.
  // *----------------------------------------------*
  // |++| Icon | Permission Title     | Combobox |++|
  // *----------------------------------------------*
  views::ColumnSet* permissions_set =
      layout->AddColumnSet(kPermissionColumnSetId);
  permissions_set->AddPaddingColumn(kFixed, side_margin);
  permissions_set->AddColumn(GridLayout::CENTER, GridLayout::CENTER, kFixed,
                             GridLayout::FIXED, kIconColumnWidth, 0);
  permissions_set->AddPaddingColumn(
      kFixed, layout_provider->GetDistanceMetric(
                  views::DISTANCE_RELATED_LABEL_HORIZONTAL));
  permissions_set->AddColumn(GridLayout::LEADING, GridLayout::CENTER, kFixed,
                             GridLayout::USE_PREF, 0, 0);
  permissions_set->AddPaddingColumn(
      1, layout_provider->GetDistanceMetric(
             views::DISTANCE_RELATED_CONTROL_HORIZONTAL));
  permissions_set->AddColumn(GridLayout::TRAILING, GridLayout::FILL, kStretchy,
                             GridLayout::USE_PREF, 0, 0);
  permissions_set->AddPaddingColumn(kFixed, side_margin);

  // |ChosenObjectRow| will layout itself, so just add the missing padding here.
  constexpr int kChosenObjectSectionId = 1;
  views::ColumnSet* chosen_object_set =
      layout->AddColumnSet(kChosenObjectSectionId);
  chosen_object_set->AddPaddingColumn(kFixed, side_margin);
  chosen_object_set->AddColumn(GridLayout::FILL, GridLayout::FILL, kStretchy,
                               GridLayout::USE_PREF, 0, 0);
  chosen_object_set->AddPaddingColumn(kFixed, side_margin);

  for (const auto& permission : permission_info_list) {
    std::unique_ptr<PermissionSelectorRow> selector =
        base::MakeUnique<PermissionSelectorRow>(
            profile_,
            web_contents() ? web_contents()->GetVisibleURL()
                           : GURL::EmptyGURL(),
            permission, layout);
    selector->AddObserver(this);
    selector_rows_.push_back(std::move(selector));
  }

  // In Harmony, ensure most comboboxes are the same width by setting them all
  // to the widest combobox size, provided it does not exceed a maximum width.
  // For selected options that are over the maximum width, allow them to assume
  // their full width. If the combobox selection is changed, this may make the
  // widths inconsistent again, but that is OK since the widths will be updated
  // on the next time the bubble is opened.
  if (ui::MaterialDesignController::IsSecondaryUiMaterial()) {
    const int maximum_width = ChromeLayoutProvider::Get()->GetDistanceMetric(
        views::DISTANCE_BUTTON_MAX_LINKABLE_WIDTH);
    int combobox_width = 0;
    for (const auto& selector : selector_rows_) {
      int curr_width = selector->GetComboboxWidth();
      if (maximum_width >= curr_width)
        combobox_width = std::max(combobox_width, curr_width);
    }
    for (const auto& selector : selector_rows_)
      selector->SetMinComboboxWidth(combobox_width);
  }

  for (auto& object : chosen_object_info_list) {
    // Since chosen objects are presented after permissions in the same list,
    // make sure its height is the same as the permissions row's minimum height
    // plus padding.
    layout->StartRow(
        1, kChosenObjectSectionId,
        PermissionSelectorRow::MinHeightForPermissionRow() + list_item_padding);
    // The view takes ownership of the object info.
    auto object_view = std::make_unique<ChosenObjectRow>(std::move(object));
    object_view->AddObserver(this);
    layout->AddView(object_view.release());
  }
  layout->AddPaddingRow(kFixed, list_item_padding);

  layout->Layout(permissions_view_);
  SizeToContents();
}

void PageInfoBubbleView::SetIdentityInfo(const IdentityInfo& identity_info) {
  std::unique_ptr<PageInfoUI::SecurityDescription> security_description =
      identity_info.GetSecurityDescription();

  // Set the bubble title, update the title label text, then apply color.
  summary_text_ = security_description->summary;
  GetBubbleFrameView()->UpdateWindowTitle();
  if (ui::MaterialDesignController::IsSecondaryUiMaterial()) {
    int text_style = views::style::STYLE_PRIMARY;
    switch (security_description->summary_style) {
      case SecuritySummaryColor::RED:
        text_style = STYLE_RED;
        break;
      case SecuritySummaryColor::GREEN:
        text_style = STYLE_GREEN;
        break;
    }
    static_cast<views::Label*>(GetBubbleFrameView()->title())
        ->SetEnabledColor(views::style::GetColor(
            *this, views::style::CONTEXT_DIALOG_TITLE, text_style));
  }

  if (identity_info.certificate) {
    certificate_ = identity_info.certificate;

    if (identity_info.show_ssl_decision_revoke_button) {
      header_->AddResetDecisionsLabel();
    }

    // Show information about the page's certificate.
    // The text of link to the Certificate Viewer varies depending on the
    // validity of the Certificate.
    const bool valid_identity =
        (identity_info.identity_status != PageInfo::SITE_IDENTITY_STATUS_ERROR);
    const base::string16 link_title = l10n_util::GetStringUTF16(
        valid_identity ? IDS_PAGE_INFO_CERTIFICATE_VALID_LINK
                       : IDS_PAGE_INFO_CERTIFICATE_INVALID_LINK);

    // Create the link to add to the Certificate Section.
    views::Link* certificate_viewer_link = new views::Link(link_title);
    certificate_viewer_link->set_id(
        PageInfoBubbleView::VIEW_ID_PAGE_INFO_LINK_CERTIFICATE_VIEWER);
    certificate_viewer_link->set_listener(this);
    certificate_viewer_link->SetUnderline(false);
    if (valid_identity) {
      certificate_viewer_link->SetTooltipText(l10n_util::GetStringFUTF16(
          IDS_PAGE_INFO_CERTIFICATE_VALID_LINK_TOOLTIP,
          base::UTF8ToUTF16(certificate_->issuer().GetDisplayName())));
    } else {
      certificate_viewer_link->SetTooltipText(l10n_util::GetStringUTF16(
          IDS_PAGE_INFO_CERTIFICATE_INVALID_LINK_TOOLTIP));
    }

    // Add the Certificate Section.
    site_settings_view_->AddChildView(CreateInspectLinkSection(
        PageInfoUI::GetCertificateIcon(), IDS_PAGE_INFO_CERTIFICATE,
        certificate_viewer_link));
  }

  if (identity_info.show_change_password_buttons) {
    header_->AddPasswordReuseButtons();
  }

  header_->SetDetails(security_description->details);

  Layout();
  SizeToContents();
}

views::View* PageInfoBubbleView::CreateSiteSettingsView(int side_margin) {
  views::View* site_settings_view = new views::View();
  views::BoxLayout* box_layout = new views::BoxLayout(
      views::BoxLayout::kVertical, gfx::Insets(0, side_margin),
      ChromeLayoutProvider::Get()->GetDistanceMetric(
          DISTANCE_CONTROL_LIST_VERTICAL));
  site_settings_view->SetLayoutManager(box_layout);
  box_layout->set_cross_axis_alignment(
      views::BoxLayout::CROSS_AXIS_ALIGNMENT_STRETCH);

  return site_settings_view;
}

void PageInfoBubbleView::HandleLinkClickedAsync(views::Link* source) {
  // All switch cases require accessing web_contents(), so we check it here.
  if (web_contents() == nullptr || web_contents()->IsBeingDestroyed()) {
    return;
  }
  switch (source->id()) {
    case PageInfoBubbleView::VIEW_ID_PAGE_INFO_LINK_SITE_SETTINGS:
      presenter_->OpenSiteSettingsView();
      break;
    case PageInfoBubbleView::VIEW_ID_PAGE_INFO_LINK_COOKIE_DIALOG:
      // Count how often the Collected Cookies dialog is opened.
      presenter_->RecordPageInfoAction(
          PageInfo::PAGE_INFO_COOKIES_DIALOG_OPENED);
      new CollectedCookiesViews(web_contents());
      break;
    case PageInfoBubbleView::VIEW_ID_PAGE_INFO_LINK_CERTIFICATE_VIEWER: {
      gfx::NativeWindow top_window = web_contents()->GetTopLevelNativeWindow();
      if (certificate_ && top_window) {
        presenter_->RecordPageInfoAction(
            PageInfo::PAGE_INFO_CERTIFICATE_DIALOG_OPENED);
        ShowCertificateViewer(web_contents(), top_window, certificate_.get());
      }
      break;
    }
    default:
      NOTREACHED();
  }
}

void PageInfoBubbleView::StyledLabelLinkClicked(views::StyledLabel* label,
                                                const gfx::Range& range,
                                                int event_flags) {
  switch (label->id()) {
    case PageInfoBubbleView::VIEW_ID_PAGE_INFO_LABEL_SECURITY_DETAILS:
      web_contents()->OpenURL(content::OpenURLParams(
          GURL(chrome::kPageInfoHelpCenterURL), content::Referrer(),
          WindowOpenDisposition::NEW_FOREGROUND_TAB, ui::PAGE_TRANSITION_LINK,
          false));
      presenter_->RecordPageInfoAction(
          PageInfo::PAGE_INFO_CONNECTION_HELP_OPENED);
      break;
    case PageInfoBubbleView::
        VIEW_ID_PAGE_INFO_LABEL_RESET_CERTIFICATE_DECISIONS:
      presenter_->OnRevokeSSLErrorBypassButtonPressed();
      GetWidget()->Close();
      break;
    default:
      NOTREACHED();
  }
}

#if !defined(OS_MACOSX) || BUILDFLAG(MAC_VIEWS_BROWSER)
void ShowPageInfoDialogImpl(Browser* browser,
                            content::WebContents* web_contents,
                            const GURL& virtual_url,
                            const security_state::SecurityInfo& security_info) {
  views::BubbleDialogDelegateView* bubble =
      PageInfoBubbleView::CreatePageInfoBubble(browser, web_contents,
                                               virtual_url, security_info);
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  bubble->GetWidget()->AddObserver(
      browser_view->GetLocationBarView()->location_icon_view());
  bubble->GetWidget()->Show();
}
#endif
