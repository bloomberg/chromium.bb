// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/custom_tab_bar_view.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/extensions/hosted_app_browser_controller.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/omnibox/omnibox_theme.h"
#include "chrome/browser/ui/page_info/page_info_dialog.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/hosted_app_button_container.h"
#include "components/url_formatter/url_formatter.h"
#include "content/public/browser/navigation_entry.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/style/typography.h"
#include "ui/views/style/typography_provider.h"

#if defined(OS_CHROMEOS)
#include "ash/public/cpp/ash_constants.h"
#else
#include "chrome/browser/themes/theme_properties.h"
#endif

namespace {

// The frame color is different on ChromeOS and other platforms because Ash
// specifies its own default frame color, which is not exposed through
// BrowserNonClientFrameView::GetFrameColor.
SkColor GetDefaultFrameColor() {
#if defined(OS_CHROMEOS)
  return ash::kDefaultFrameColor;
#else
  return ThemeProperties::GetDefaultColor(ThemeProperties::COLOR_FRAME, false);
#endif
}

}  // namespace

// Container view for laying out and rendering the title/origin of the current
// page.
class CustomTabBarTitleOriginView : public views::View {
 public:
  explicit CustomTabBarTitleOriginView(SkColor text_color) {
    title_label_ = new views::Label(
        base::string16(), views::style::TextContext::CONTEXT_DIALOG_TITLE);
    location_label_ = new views::Label(base::string16());

    // We need to disable auto color readability, as we want to match the active
    // color in the title bar, which is subtly different.
    // TODO(http://crbug.com/883177): Enable this if we use GetReadableColor(..)
    // for the app title text instead of GetThemedAssetColor(..).
    title_label_->SetAutoColorReadabilityEnabled(false);
    location_label_->SetAutoColorReadabilityEnabled(false);

    title_label_->SetEnabledColor(text_color);
    location_label_->SetEnabledColor(text_color);

    AddChildView(title_label_);
    AddChildView(location_label_);

    auto layout = std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kVertical, gfx::Insets(), 0);
    layout->set_cross_axis_alignment(
        views::BoxLayout::CrossAxisAlignment::CROSS_AXIS_ALIGNMENT_START);
    SetLayoutManager(std::move(layout));
  }

  void Update(base::string16 title, base::string16 location) {
    title_label_->SetText(title);
    location_label_->SetText(location);
  }

 private:
  views::Label* title_label_;
  views::Label* location_label_;
};

// static
const char CustomTabBarView::kViewClassName[] = "CustomTabBarView";

CustomTabBarView::CustomTabBarView(BrowserView* browser_view,
                                   LocationBarView::Delegate* delegate)
    : TabStripModelObserver(),
      delegate_(delegate),
      tab_strip_model_observer_(this) {
  Browser* browser = browser_view->browser();
  base::Optional<SkColor> optional_theme_color =
      browser->hosted_app_controller()->GetThemeColor();

  // If we have a theme color, use that, otherwise fall back to the default
  // frame color.
  theme_color_ = optional_theme_color.value_or(GetDefaultFrameColor());
  SetBackground(views::CreateSolidBackground(theme_color_));

  text_color_ = browser_view->frame()->GetFrameView()->GetFrameForegroundColor(
      BrowserNonClientFrameView::kActive);

  const gfx::FontList& font_list = views::style::GetFont(
      CONTEXT_OMNIBOX_PRIMARY, views::style::STYLE_PRIMARY);

  location_icon_view_ = new LocationIconView(font_list, this);
  AddChildView(location_icon_view_);

  title_origin_view_ = new CustomTabBarTitleOriginView(text_color_);
  AddChildView(title_origin_view_);

  int padding = GetLayoutConstant(LayoutConstant::LOCATION_BAR_ELEMENT_PADDING);
  // The location icon already has some padding, so we subtract it from the
  // padding we're going to apply.
  int location_icon_padding =
      GetLayoutInsets(LayoutInset::LOCATION_BAR_ICON_INTERIOR_PADDING).left();
  gfx::Insets insets(padding, padding - location_icon_padding, padding,
                     padding);

  auto layout = std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, insets, 0);
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::CROSS_AXIS_ALIGNMENT_CENTER);

  SetLayoutManager(std::move(layout));
  SetBorder(views::CreateSolidSidedBorder(0, 0, 1, 0, text_color_));

  tab_strip_model_observer_.Add(browser->tab_strip_model());
}

CustomTabBarView::~CustomTabBarView() {}

void CustomTabBarView::TabChangedAt(content::WebContents* contents,
                                    int index,
                                    TabChangeType change_type) {
  if (!contents)
    return;

  content::NavigationEntry* entry = contents->GetController().GetVisibleEntry();
  base::string16 title, location;
  if (entry) {
    title = Browser::FormatTitleForDisplay(entry->GetTitleForDisplay());
    location = url_formatter::FormatUrl(
        entry->GetVirtualURL(), url_formatter::kFormatUrlOmitDefaults,
        net::UnescapeRule::NORMAL, nullptr, nullptr, nullptr);
  }

  title_origin_view_->Update(title, location);
  location_icon_view_->Update(/*suppress animations = */ false);

  last_title_ = title;
  last_location_ = location;

  Layout();
}

content::WebContents* CustomTabBarView::GetWebContents() {
  return delegate_->GetWebContents();
}

bool CustomTabBarView::IsEditingOrEmpty() {
  return false;
}

void CustomTabBarView::OnLocationIconPressed(const ui::MouseEvent& event) {}

void CustomTabBarView::OnLocationIconDragged(const ui::MouseEvent& event) {}

bool CustomTabBarView::ShowPageInfoDialog() {
  return ::ShowPageInfoDialog(GetWebContents(),
                              bubble_anchor_util::Anchor::kCustomTabBar);
}

SkColor CustomTabBarView::GetSecurityChipColor(
    security_state::SecurityLevel security_level) const {
  OmniboxTint tint =
      color_utils::IsDark(text_color_) ? OmniboxTint::LIGHT : OmniboxTint::DARK;

  return GetOmniboxSecurityChipColor(tint, security_level);
}

gfx::ImageSkia CustomTabBarView::GetLocationIcon(
    LocationIconView::Delegate::IconFetchedCallback on_icon_fetched) const {
  return gfx::CreateVectorIcon(
      delegate_->GetLocationBarModel()->GetVectorIcon(),
      GetLayoutConstant(LOCATION_BAR_ICON_SIZE),
      GetSecurityChipColor(GetLocationBarModel()->GetSecurityLevel(false)));
}

SkColor CustomTabBarView::GetLocationIconInkDropColor() const {
  return GetNativeTheme()->GetSystemColor(
      ui::NativeTheme::kColorId_TextfieldDefaultColor);
}

const LocationBarModel* CustomTabBarView::GetLocationBarModel() const {
  return delegate_->GetLocationBarModel();
}
