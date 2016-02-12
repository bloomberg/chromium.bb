// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/website_settings/website_settings_popup_view.h"

#include <stddef.h>

#include <algorithm>
#include <vector>

#include "base/i18n/rtl.h"
#include "base/macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/certificate_viewer.h"
#include "chrome/browser/devtools/devtools_toggle_action.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/views/collected_cookies_views.h"
#include "chrome/browser/ui/views/website_settings/chosen_object_view.h"
#include "chrome/browser/ui/views/website_settings/permission_selector_view.h"
#include "chrome/browser/ui/website_settings/website_settings.h"
#include "chrome/browser/ui/website_settings/website_settings_utils.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/cert_store.h"
#include "content/public/browser/user_metrics.h"
#include "grit/components_chromium_strings.h"
#include "grit/components_google_chrome_strings.h"
#include "grit/components_strings.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/image/image.h"
#include "ui/resources/grit/ui_resources.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/grid_layout.h"
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
bool is_popup_showing = false;

// Left icon margin.
const int kIconMarginLeft = 6;

// Margin and padding values for the |PopupHeaderView|.
const int kHeaderMarginBottom = 10;
const int kHeaderPaddingBottom = 16;
const int kHeaderPaddingLeft = 18;
const int kHeaderPaddingRightForCloseButton = 8;
const int kHeaderPaddingRightForText = kHeaderPaddingLeft;
const int kHeaderPaddingTop = 12;

// Spacing between the site identity label and the site identity status text in
// the popup header.
const int kHeaderRowSpacing = 4;

// To make the bubble's arrow point directly at the location icon rather than at
// the Omnibox's edge, inset the bubble's anchor rect by this amount of pixels.
const int kLocationIconVerticalMargin = 5;

// The max possible width of the popup.
const int kMaxPopupWidth = 1000;

// The margins between the popup border and the popup content.
const int kPopupMarginTop = 4;
const int kPopupMarginLeft = 0;
const int kPopupMarginBottom = 14;
const int kPopupMarginRight = 0;

// Padding values for sections on the site settings view.
const int kSiteSettingsViewContentMinWidth = 300;
const int kSiteSettingsViewPaddingBottom = 6;
const int kSiteSettingsViewPaddingLeft = 18;
const int kSiteSettingsViewPaddingRight = 18;
const int kSiteSettingsViewPaddingTop = 4;

// Space between the headline and the content of a section.
const int kSiteSettingsViewHeadlineMarginBottom = 10;
// Spacing between rows in the "Permissions" and "Cookies and Site Data"
// sections.
const int kContentRowSpacing = 2;

const int kSiteDataIconColumnWidth = 20;

const int BUTTON_RESET_CERTIFICATE_DECISIONS = 1337;

}  // namespace

// |PopupHeaderView| is the UI element (view) that represents the header of the
// |WebsiteSettingsPopupView|. The header shows the status of the site's
// identity check and the name of the site's identity.
class PopupHeaderView : public views::View {
 public:
  explicit PopupHeaderView(views::ButtonListener* button_listener,
                           views::StyledLabelListener* styled_label_listener);
  ~PopupHeaderView() override;

  // Sets the name of the site's identity.
  void SetIdentityName(const base::string16& name);

  // Sets the security summary text for the current page.
  void SetSecuritySummary(const base::string16& security_summary_text,
                          bool include_details_link);

  int GetPreferredNameWidth() const;

  void AddResetDecisionsButton();

 private:
  // The label that displays the name of the site's identity.
  views::Label* name_;
  // The label that displays the status of the identity check for this site.
  // Includes a link to open the DevTools Security panel.
  views::StyledLabel* status_;

  // The button listener attached to the buttons in this view.
  views::ButtonListener* button_listener_;

  // A container for the button for resetting cert decisions. The button is only
  // shown sometimes, so we use a container to keep track of where to place it
  // (if needed).
  views::View* reset_decisions_button_container_;

  DISALLOW_COPY_AND_ASSIGN(PopupHeaderView);
};

// Website Settings are not supported for internal Chrome pages. Instead of the
// |WebsiteSettingsPopupView|, the |InternalPageInfoPopupView| is
// displayed.
class InternalPageInfoPopupView : public views::BubbleDelegateView {
 public:
  // If |anchor_view| is nullptr, or has no Widget, |parent_window| may be
  // provided to ensure this bubble is closed when the parent closes.
  InternalPageInfoPopupView(views::View* anchor_view,
                            gfx::NativeView parent_window);
  ~InternalPageInfoPopupView() override;

  // views::BubbleDelegateView:
  views::NonClientFrameView* CreateNonClientFrameView(
      views::Widget* widget) override;
  void OnWidgetDestroying(views::Widget* widget) override;

 private:
  friend class WebsiteSettingsPopupView;

  DISALLOW_COPY_AND_ASSIGN(InternalPageInfoPopupView);
};

////////////////////////////////////////////////////////////////////////////////
// Popup Header
////////////////////////////////////////////////////////////////////////////////

PopupHeaderView::PopupHeaderView(
    views::ButtonListener* button_listener,
    views::StyledLabelListener* styled_label_listener)
    : name_(nullptr),
      status_(nullptr),
      button_listener_(button_listener),
      reset_decisions_button_container_(nullptr) {
  views::GridLayout* layout = new views::GridLayout(this);
  SetLayoutManager(layout);

  const int label_column = 0;
  views::ColumnSet* column_set = layout->AddColumnSet(label_column);
  column_set->AddPaddingColumn(0, kHeaderPaddingLeft);
  column_set->AddColumn(views::GridLayout::FILL,
                        views::GridLayout::FILL,
                        1,
                        views::GridLayout::USE_PREF,
                        0,
                        0);
  column_set->AddPaddingColumn(1, 0);
  column_set->AddColumn(views::GridLayout::FILL,
                        views::GridLayout::FILL,
                        1,
                        views::GridLayout::USE_PREF,
                        0,
                        0);
  column_set->AddPaddingColumn(0, kHeaderPaddingRightForCloseButton);

  layout->AddPaddingRow(0, kHeaderPaddingTop);

  layout->StartRow(0, label_column);
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  name_ = new views::Label(
      base::string16(), rb.GetFontList(ui::ResourceBundle::BoldFont));
  layout->AddView(name_, 1, 1, views::GridLayout::LEADING,
                  views::GridLayout::TRAILING);
  views::ImageButton* close_button = new views::ImageButton(button_listener);
  close_button->SetImage(views::CustomButton::STATE_NORMAL,
                         rb.GetImageNamed(IDR_CLOSE_2).ToImageSkia());
  close_button->SetImage(views::CustomButton::STATE_HOVERED,
                         rb.GetImageNamed(IDR_CLOSE_2_H).ToImageSkia());
  close_button->SetImage(views::CustomButton::STATE_PRESSED,
                         rb.GetImageNamed(IDR_CLOSE_2_P).ToImageSkia());
  layout->AddView(close_button, 1, 1, views::GridLayout::TRAILING,
                  views::GridLayout::LEADING);

  layout->AddPaddingRow(0, kHeaderRowSpacing);

  const int label_column_status = 1;
  views::ColumnSet* column_set_status =
      layout->AddColumnSet(label_column_status);
  column_set_status->AddPaddingColumn(0, kHeaderPaddingLeft);
  column_set_status->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL,
                               1, views::GridLayout::USE_PREF, 0, 0);
  column_set_status->AddPaddingColumn(0, kHeaderPaddingRightForText);

  layout->StartRow(0, label_column_status);
  status_ = new views::StyledLabel(base::string16(), styled_label_listener);
  layout->AddView(status_,
                  1,
                  1,
                  views::GridLayout::LEADING,
                  views::GridLayout::LEADING);

  layout->StartRow(0, label_column_status);
  reset_decisions_button_container_ = new views::View();
  reset_decisions_button_container_->SetLayoutManager(
      new views::BoxLayout(views::BoxLayout::kHorizontal, 0, 0, 0));
  layout->AddView(reset_decisions_button_container_, 1, 1,
                  views::GridLayout::LEADING, views::GridLayout::LEADING);

  layout->AddPaddingRow(1, kHeaderPaddingBottom);
}

PopupHeaderView::~PopupHeaderView() {}

int PopupHeaderView::GetPreferredNameWidth() const {
  return name_->GetPreferredSize().width();
}

void PopupHeaderView::SetIdentityName(const base::string16& name) {
  name_->SetText(name);
}

void PopupHeaderView::SetSecuritySummary(
    const base::string16& security_summary_text,
    bool include_details_link) {
  if (include_details_link) {
    base::string16 details_string =
        l10n_util::GetStringUTF16(IDS_WEBSITE_SETTINGS_DETAILS_LINK);

    std::vector<base::string16> subst;
    subst.push_back(security_summary_text);
    subst.push_back(details_string);

    std::vector<size_t> offsets;

    base::string16 text = base::ReplaceStringPlaceholders(
        base::ASCIIToUTF16("$1 $2"), subst, &offsets);
    status_->SetText(text);
    gfx::Range details_range(offsets[1], text.length());

    views::StyledLabel::RangeStyleInfo link_style =
        views::StyledLabel::RangeStyleInfo::CreateForLink();
    link_style.font_style |= gfx::Font::FontStyle::UNDERLINE;
    link_style.disable_line_wrapping = false;

    status_->AddStyleRange(details_range, link_style);
  } else {
    status_->SetText(security_summary_text);
  }

  // Fit the styled label to occupy available width.
  status_->SizeToFit(0);
}

void PopupHeaderView::AddResetDecisionsButton() {
  views::LabelButton* reset_decisions_button = new views::LabelButton(
      button_listener_,
      l10n_util::GetStringUTF16(
          IDS_PAGEINFO_RESET_INVALID_CERTIFICATE_DECISIONS_BUTTON));
  reset_decisions_button->set_id(BUTTON_RESET_CERTIFICATE_DECISIONS);
  reset_decisions_button->SetStyle(views::Button::STYLE_BUTTON);

  reset_decisions_button_container_->AddChildView(reset_decisions_button);

  // Now that it contains a button, the container needs padding at the top.
  reset_decisions_button_container_->SetBorder(
      views::Border::CreateEmptyBorder(8, 0, 0, 0));

  InvalidateLayout();
}

////////////////////////////////////////////////////////////////////////////////
// InternalPageInfoPopupView
////////////////////////////////////////////////////////////////////////////////

InternalPageInfoPopupView::InternalPageInfoPopupView(
    views::View* anchor_view,
    gfx::NativeView parent_window)
    : BubbleDelegateView(anchor_view, views::BubbleBorder::TOP_LEFT) {
  set_parent_window(parent_window);

  // Compensate for built-in vertical padding in the anchor view's image.
  set_anchor_view_insets(gfx::Insets(kLocationIconVerticalMargin, 0,
                                     kLocationIconVerticalMargin, 0));

  const int kSpacing = 16;
  SetLayoutManager(new views::BoxLayout(views::BoxLayout::kHorizontal, kSpacing,
                                        kSpacing, kSpacing));
  set_margins(gfx::Insets());
  views::ImageView* icon_view = new views::ImageView();
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  icon_view->SetImage(rb.GetImageSkiaNamed(IDR_PRODUCT_LOGO_16));
  AddChildView(icon_view);

  views::Label* label =
      new views::Label(l10n_util::GetStringUTF16(IDS_PAGE_INFO_INTERNAL_PAGE));
  label->SetMultiLine(true);
  label->SetAllowCharacterBreak(true);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  AddChildView(label);

  views::BubbleDelegateView::CreateBubble(this);
}

InternalPageInfoPopupView::~InternalPageInfoPopupView() {
}

views::NonClientFrameView* InternalPageInfoPopupView::CreateNonClientFrameView(
    views::Widget* widget) {
  views::BubbleFrameView* frame = static_cast<views::BubbleFrameView*>(
      BubbleDelegateView::CreateNonClientFrameView(widget));
  // 16px padding + half of icon width comes out to 24px.
  frame->bubble_border()->set_arrow_offset(
      24 + frame->bubble_border()->GetBorderThickness());
  return frame;
}

void InternalPageInfoPopupView::OnWidgetDestroying(views::Widget* widget) {
  is_popup_showing = false;
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
    const security_state::SecurityStateModel::SecurityInfo& security_info) {
  is_popup_showing = true;
  gfx::NativeView parent_window =
      anchor_view ? nullptr : web_contents->GetNativeView();
  if (InternalChromePage(url)) {
    // Use the concrete type so that |SetAnchorRect| can be called as a friend.
    InternalPageInfoPopupView* popup =
        new InternalPageInfoPopupView(anchor_view, parent_window);
    if (!anchor_view)
      popup->SetAnchorRect(anchor_rect);
    popup->GetWidget()->Show();
  } else {
    WebsiteSettingsPopupView* popup = new WebsiteSettingsPopupView(
        anchor_view, parent_window, profile, web_contents, url, security_info);
    if (!anchor_view)
      popup->SetAnchorRect(anchor_rect);
    popup->GetWidget()->Show();
  }
}

// static
bool WebsiteSettingsPopupView::IsPopupShowing() {
  return is_popup_showing;
}

WebsiteSettingsPopupView::WebsiteSettingsPopupView(
    views::View* anchor_view,
    gfx::NativeView parent_window,
    Profile* profile,
    content::WebContents* web_contents,
    const GURL& url,
    const security_state::SecurityStateModel::SecurityInfo& security_info)
    : content::WebContentsObserver(web_contents),
      BubbleDelegateView(anchor_view, views::BubbleBorder::TOP_LEFT),
      web_contents_(web_contents),
      header_(nullptr),
      separator_(nullptr),
      site_settings_view_(nullptr),
      site_data_content_(nullptr),
      cookie_dialog_link_(nullptr),
      permissions_content_(nullptr),
      cert_id_(0),
      site_settings_link_(nullptr),
      weak_factory_(this) {
  set_parent_window(parent_window);

  is_devtools_disabled_ =
      profile->GetPrefs()->GetBoolean(prefs::kDevToolsDisabled);

  // Compensate for built-in vertical padding in the anchor view's image.
  set_anchor_view_insets(gfx::Insets(kLocationIconVerticalMargin, 0,
                                     kLocationIconVerticalMargin, 0));

  views::GridLayout* layout = new views::GridLayout(this);
  SetLayoutManager(layout);
  const int content_column = 0;
  views::ColumnSet* column_set = layout->AddColumnSet(content_column);
  column_set->AddColumn(views::GridLayout::FILL,
                        views::GridLayout::FILL,
                        1,
                        views::GridLayout::USE_PREF,
                        0,
                        0);

  header_ = new PopupHeaderView(this, this);
  layout->StartRow(1, content_column);
  layout->AddView(header_);

  layout->StartRow(0, content_column);
  separator_ = new views::Separator(views::Separator::HORIZONTAL);
  layout->AddView(separator_);

  layout->AddPaddingRow(1, kHeaderMarginBottom);
  layout->StartRow(1, content_column);

  site_settings_view_ = CreateSiteSettingsView();
  layout->AddView(site_settings_view_);

  set_margins(gfx::Insets(kPopupMarginTop, kPopupMarginLeft,
                          kPopupMarginBottom, kPopupMarginRight));

  views::BubbleDelegateView::CreateBubble(this);

  presenter_.reset(new WebsiteSettings(
      this, profile, TabSpecificContentSettings::FromWebContents(web_contents),
      web_contents, url, security_info, content::CertStore::GetInstance()));
}

void WebsiteSettingsPopupView::RenderFrameDeleted(
    content::RenderFrameHost* render_frame_host) {
  if (render_frame_host == web_contents_->GetMainFrame()) {
    GetWidget()->Close();
  }
}

void WebsiteSettingsPopupView::OnPermissionChanged(
    const WebsiteSettingsUI::PermissionInfo& permission) {
  presenter_->OnSitePermissionChanged(permission.type, permission.setting);
}

void WebsiteSettingsPopupView::OnChosenObjectDeleted(
    const WebsiteSettingsUI::ChosenObjectInfo& info) {
  presenter_->OnSiteChosenObjectDeleted(info.ui_info, *info.object);
}

void WebsiteSettingsPopupView::OnWidgetDestroying(views::Widget* widget) {
  is_popup_showing = false;
  presenter_->OnUIClosing();
}

void WebsiteSettingsPopupView::ButtonPressed(views::Button* button,
                                             const ui::Event& event) {
  if (button->id() == BUTTON_RESET_CERTIFICATE_DECISIONS)
    presenter_->OnRevokeSSLErrorBypassButtonPressed();
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

  int width = kSiteSettingsViewContentMinWidth;
  if (site_data_content_)
    width = std::max(width, site_data_content_->GetPreferredSize().width());
  if (permissions_content_)
    width = std::max(width, permissions_content_->GetPreferredSize().width());
  if (header_)
    width = std::max(width, header_->GetPreferredNameWidth());
  width += kSiteSettingsViewPaddingLeft + kSiteSettingsViewPaddingRight;
  width = std::min(width, kMaxPopupWidth);
  return gfx::Size(width, height);
}

void WebsiteSettingsPopupView::SetCookieInfo(
    const CookieInfoList& cookie_info_list) {
  // |cookie_info_list| should only ever have 2 items: first- and third-party
  // cookies.
  DCHECK_EQ(cookie_info_list.size(), 2u);
  base::string16 first_party_label_text;
  base::string16 third_party_label_text;
  for (const auto& i : cookie_info_list) {
    if (i.is_first_party) {
      first_party_label_text =
          l10n_util::GetStringFUTF16(IDS_WEBSITE_SETTINGS_FIRST_PARTY_SITE_DATA,
                                     base::IntToString16(i.allowed));
    } else {
      third_party_label_text =
          l10n_util::GetStringFUTF16(IDS_WEBSITE_SETTINGS_THIRD_PARTY_SITE_DATA,
                                     base::IntToString16(i.allowed));
    }
  }

  if (!cookie_dialog_link_) {
    cookie_dialog_link_ = new views::Link(first_party_label_text);
    cookie_dialog_link_->set_listener(this);
  } else {
    cookie_dialog_link_->SetText(first_party_label_text);
  }

  views::GridLayout* layout =
      static_cast<views::GridLayout*>(site_data_content_->GetLayoutManager());
  if (!layout) {
    layout = new views::GridLayout(site_data_content_);
    site_data_content_->SetLayoutManager(layout);

    const int site_data_content_column = 0;
    views::ColumnSet* column_set =
        layout->AddColumnSet(site_data_content_column);
    column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL, 1,
                          views::GridLayout::FIXED, kSiteDataIconColumnWidth,
                          0);
    column_set->AddPaddingColumn(0, kIconMarginLeft);
    column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL, 1,
                          views::GridLayout::USE_PREF, 0, 0);
    // No padding. This third column is for |third_party_label_text| (see
    // below),
    // and the text needs to flow naturally from the |first_party_label_text|
    // link.
    column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL, 1,
                          views::GridLayout::USE_PREF, 0, 0);

    layout->AddPaddingRow(1, 5);

    layout->StartRow(1, site_data_content_column);
    WebsiteSettingsUI::PermissionInfo info;
    info.type = CONTENT_SETTINGS_TYPE_COOKIES;
    info.setting = CONTENT_SETTING_ALLOW;
    info.is_incognito =
        Profile::FromBrowserContext(web_contents_->GetBrowserContext())
            ->IsOffTheRecord();
    views::ImageView* icon = new views::ImageView();
    const gfx::Image& image = WebsiteSettingsUI::GetPermissionIcon(info);
    icon->SetImage(image.ToImageSkia());
    layout->AddView(icon, 1, 1, views::GridLayout::CENTER,
                    views::GridLayout::CENTER);
    layout->AddView(cookie_dialog_link_, 1, 1, views::GridLayout::CENTER,
                    views::GridLayout::CENTER);
    base::string16 comma = base::ASCIIToUTF16(", ");

    layout->AddView(new views::Label(comma + third_party_label_text), 1, 1,
                    views::GridLayout::LEADING, views::GridLayout::CENTER);

    layout->AddPaddingRow(1, 6);
  }

  layout->Layout(site_data_content_);
  SizeToContents();
}

void WebsiteSettingsPopupView::SetPermissionInfo(
    const PermissionInfoList& permission_info_list,
    const ChosenObjectInfoList& chosen_object_info_list) {
  // When a permission is changed, WebsiteSettings::OnSitePermissionChanged()
  // calls this method with updated permissions. However, PermissionSelectorView
  // will have already updated its state, so it's already reflected in the UI.
  // In addition, if a permission is set to the default setting, WebsiteSettings
  // removes it from |permission_info_list|, but the button should remain.
  if (permissions_content_) {
    STLDeleteContainerPointers(chosen_object_info_list.begin(),
                               chosen_object_info_list.end());
    return;
  }

  permissions_content_ = new views::View();
  views::GridLayout* layout = new views::GridLayout(permissions_content_);
  permissions_content_->SetLayoutManager(layout);

  base::string16 headline =
      permission_info_list.empty()
          ? base::string16()
          : l10n_util::GetStringUTF16(
                IDS_WEBSITE_SETTINGS_TITLE_SITE_PERMISSIONS);
  views::View* permissions_section =
      CreateSection(headline, permissions_content_, nullptr);
  site_settings_view_->AddChildView(permissions_section);

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
    PermissionSelectorView* selector = new PermissionSelectorView(
        web_contents_ ? web_contents_->GetURL() : GURL::EmptyGURL(),
        permission);
    selector->AddObserver(this);
    layout->AddView(selector,
                    1,
                    1,
                    views::GridLayout::LEADING,
                    views::GridLayout::CENTER);
    layout->AddPaddingRow(1, kContentRowSpacing);
  }

  for (auto object : chosen_object_info_list) {
    layout->StartRow(1, content_column);
    // The view takes ownership of the object info.
    auto object_view = new ChosenObjectView(make_scoped_ptr(object));
    object_view->AddObserver(this);
    layout->AddView(object_view, 1, 1, views::GridLayout::LEADING,
                    views::GridLayout::CENTER);
    layout->AddPaddingRow(1, kContentRowSpacing);
  }

  layout->Layout(permissions_content_);

  // Add site settings link.
  site_settings_link_ = new views::Link(
      l10n_util::GetStringUTF16(IDS_PAGE_INFO_SITE_SETTINGS_LINK));
  site_settings_link_->set_listener(this);
  views::View* link_section = new views::View();
  const int kLinkMarginTop = 4;
  link_section->SetLayoutManager(
      new views::BoxLayout(views::BoxLayout::kHorizontal,
                           kSiteSettingsViewPaddingLeft, kLinkMarginTop, 0));
  link_section->AddChildView(site_settings_link_);
  site_settings_view_->AddChildView(link_section);

  SizeToContents();
}

void WebsiteSettingsPopupView::SetIdentityInfo(
    const IdentityInfo& identity_info) {
  base::string16 security_summary_text = identity_info.GetSecuritySummary();
  header_->SetIdentityName(base::UTF8ToUTF16(identity_info.site_identity));

  if (identity_info.cert_id) {
    cert_id_ = identity_info.cert_id;

    if (identity_info.show_ssl_decision_revoke_button)
      header_->AddResetDecisionsButton();
  }

  header_->SetSecuritySummary(security_summary_text,
                              identity_info.cert_id != 0);

  Layout();
  SizeToContents();
}

void WebsiteSettingsPopupView::SetSelectedTab(TabId tab_id) {
  // TODO(lgarron): Remove this method. (https://crbug.com/571533)
}

views::View* WebsiteSettingsPopupView::CreateSiteSettingsView() {
  views::View* pane = new views::View();
  pane->SetLayoutManager(
      new views::BoxLayout(views::BoxLayout::kVertical, 0, 0, 1));

  // Add cookies and site data section.
  site_data_content_ = new views::View();
  views::View* site_data_section = CreateSection(
      l10n_util::GetStringUTF16(IDS_WEBSITE_SETTINGS_TITLE_SITE_DATA),
      site_data_content_, NULL);
  pane->AddChildView(site_data_section);

  return pane;
}
views::View* WebsiteSettingsPopupView::CreateSection(
    const base::string16& headline_text,
    views::View* content,
    views::Link* link) {
  views::View* container = new views::View();
  views::GridLayout* layout = new views::GridLayout(container);
  container->SetLayoutManager(layout);
  const int content_column = 0;
  views::ColumnSet* column_set = layout->AddColumnSet(content_column);
  column_set->AddPaddingColumn(0, kSiteSettingsViewPaddingLeft);
  column_set->AddColumn(views::GridLayout::FILL,
                        views::GridLayout::FILL,
                        1,
                        views::GridLayout::USE_PREF,
                        0,
                        0);

  if (headline_text.length() > 0) {
    layout->AddPaddingRow(1, kSiteSettingsViewPaddingTop);
    layout->StartRow(1, content_column);
    ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
    views::Label* headline = new views::Label(
        headline_text, rb.GetFontList(ui::ResourceBundle::BoldFont));
    layout->AddView(headline, 1, 1, views::GridLayout::LEADING,
                    views::GridLayout::CENTER);
  }

  layout->AddPaddingRow(1, kSiteSettingsViewHeadlineMarginBottom);
  layout->StartRow(1, content_column);
  layout->AddView(content, 1, 1, views::GridLayout::LEADING,
                  views::GridLayout::CENTER);

  if (link) {
    layout->AddPaddingRow(1, 4);
    layout->StartRow(1, content_column);
    layout->AddView(link, 1, 1, views::GridLayout::LEADING,
                    views::GridLayout::CENTER);
  }

  layout->AddPaddingRow(1, kSiteSettingsViewPaddingBottom);
  return container;
}


void WebsiteSettingsPopupView::HandleLinkClickedAsync(views::Link* source) {
  if (source == cookie_dialog_link_) {
    // Count how often the Collected Cookies dialog is opened.
    presenter_->RecordWebsiteSettingsAction(
        WebsiteSettings::WEBSITE_SETTINGS_COOKIES_DIALOG_OPENED);

    if (web_contents_ != NULL)
      new CollectedCookiesViews(web_contents_);
  } else if (source == site_settings_link_) {
    // TODO(palmer): This opens the general Content Settings pane, which is OK
    // for now. But on Android, it opens a page specific to a given origin that
    // shows all of the settings for that origin. If/when that's available on
    // desktop we should link to that here, too.
    web_contents_->OpenURL(content::OpenURLParams(
        GURL(chrome::kChromeUIContentSettingsURL), content::Referrer(),
        NEW_FOREGROUND_TAB, ui::PAGE_TRANSITION_LINK, false));
    presenter_->RecordWebsiteSettingsAction(
        WebsiteSettings::WEBSITE_SETTINGS_SITE_SETTINGS_OPENED);
  } else {
    NOTREACHED();
  }
}

void WebsiteSettingsPopupView::StyledLabelLinkClicked(views::StyledLabel* label,
                                                      const gfx::Range& range,
                                                      int event_flags) {
  presenter_->RecordWebsiteSettingsAction(
      WebsiteSettings::WEBSITE_SETTINGS_SECURITY_DETAILS_OPENED);

  if (is_devtools_disabled_) {
    DCHECK_NE(cert_id_, 0);
    gfx::NativeWindow parent =
        anchor_widget() ? anchor_widget()->GetNativeWindow() : nullptr;
    presenter_->RecordWebsiteSettingsAction(
        WebsiteSettings::WEBSITE_SETTINGS_CERTIFICATE_DIALOG_OPENED);
    ShowCertificateViewerByID(web_contents_, parent, cert_id_);
  } else {
    DevToolsWindow::OpenDevToolsWindow(
        web_contents_, DevToolsToggleAction::ShowSecurityPanel());
  }
}
