// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/custom_tab_bar_view.h"

#include <memory>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/extensions/hosted_app_browser_controller.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/omnibox/omnibox_theme.h"
#include "chrome/browser/ui/page_info/page_info_dialog.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/hosted_app_button_container.h"
#include "components/url_formatter/url_formatter.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/style/typography.h"
#include "ui/views/style/typography_provider.h"
#include "ui/views/view_properties.h"
#include "url/gurl.h"

#if defined(OS_CHROMEOS)
#include "ash/public/cpp/ash_constants.h"
#else
#include "chrome/browser/themes/theme_properties.h"
#endif

namespace {

constexpr SkColor kCustomTabBarViewBackgroundColor = SK_ColorWHITE;

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

views::ImageButton* CreateCloseButton(views::ButtonListener* listener,
                                      SkColor color) {
  views::ImageButton* close_button = CreateVectorImageButton(listener);
  SetImageFromVectorIconWithColor(close_button, vector_icons::kCloseRoundedIcon,
                                  color);
  close_button->SetTooltipText(l10n_util::GetStringUTF16(IDS_APP_CLOSE));
  close_button->SizeToPreferredSize();

  // Use a circular ink drop.
  auto highlight_path = std::make_unique<SkPath>();
  highlight_path->addOval(gfx::RectToSkRect(gfx::Rect(close_button->size())));
  close_button->SetProperty(views::kHighlightPathKey, highlight_path.release());

  return close_button;
}

void GoBackToApp(content::WebContents* web_contents) {
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
  GURL launch_url = browser->hosted_app_controller()->GetAppLaunchURL();
  content::NavigationController& controller = web_contents->GetController();
  content::BrowserContext* context = web_contents->GetBrowserContext();

  content::NavigationEntry* entry = nullptr;
  int offset = 0;

  // Go back until we find an in scope url, or run out of urls.
  while ((entry = controller.GetEntryAtOffset(offset)) &&
         !extensions::IsSameScope(entry->GetURL(), launch_url, context)) {
    offset--;
  }

  // If there are no in scope urls, push the app's launch url and clear
  // the history.
  if (!entry) {
    content::NavigationController::LoadURLParams load(launch_url);
    load.should_clear_history_list = true;
    controller.LoadURLWithParams(load);
    return;
  }

  // Otherwise, go back to the first in scope url.
  controller.GoToOffset(offset);
}

}  // namespace

// Container view for laying out and rendering the title/origin of the current
// page.
class CustomTabBarTitleOriginView : public views::View {
 public:
  explicit CustomTabBarTitleOriginView(SkColor foreground_color,
                                       SkColor background_color) {
    title_label_ = new views::Label(
        base::string16(), views::style::TextContext::CONTEXT_DIALOG_TITLE);
    location_label_ = new views::Label(base::string16());

    title_label_->SetEnabledColor(foreground_color);
    title_label_->SetBackgroundColor(background_color);
    title_label_->SetElideBehavior(gfx::ElideBehavior::ELIDE_TAIL);

    location_label_->SetEnabledColor(foreground_color);
    location_label_->SetBackgroundColor(background_color);
    location_label_->SetElideBehavior(gfx::ElideBehavior::ELIDE_TAIL);

    AddChildView(title_label_);
    AddChildView(location_label_);

    auto layout = std::make_unique<views::FlexLayout>();
    layout->SetOrientation(views::LayoutOrientation::kVertical)
        .SetMainAxisAlignment(views::LayoutAlignment::kCenter)
        .SetCrossAxisAlignment(views::LayoutAlignment::kStart);

    SetLayoutManager(std::move(layout));
  }

  void Update(base::string16 title, base::string16 location) {
    title_label_->SetText(title);
    location_label_->SetText(location);
  }

  // views::View:
  gfx::Size GetMinimumSize() const override {
    // As labels are not multi-line, the layout will calculate a minimum size
    // that would fit the entire text (potentially a long url). Instead, set a
    // minimum number of characters we want to display and elide the text if it
    // overflows.
    constexpr int kMinCharacters = 20;
    return gfx::Size(
        title_label_->font_list().GetExpectedTextWidth(kMinCharacters),
        GetPreferredSize().height());
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
  title_bar_color_ = optional_theme_color.value_or(GetDefaultFrameColor());
  SetBackground(views::CreateSolidBackground(kCustomTabBarViewBackgroundColor));

  constexpr SkColor kForegroundColor = gfx::kGoogleGrey900;

  const gfx::FontList& font_list = views::style::GetFont(
      CONTEXT_OMNIBOX_PRIMARY, views::style::STYLE_PRIMARY);

  close_button_ = CreateCloseButton(this, kForegroundColor);
  AddChildView(close_button_);

  location_icon_view_ = new LocationIconView(font_list, this);
  AddChildView(location_icon_view_);

  title_origin_view_ = new CustomTabBarTitleOriginView(
      kForegroundColor, kCustomTabBarViewBackgroundColor);
  AddChildView(title_origin_view_);

  int padding = GetLayoutConstant(LayoutConstant::LOCATION_BAR_ELEMENT_PADDING);
  // The location icon already has some padding, so we subtract it from the
  // padding we're going to apply.
  int location_icon_padding =
      GetLayoutInsets(LayoutInset::LOCATION_BAR_ICON_INTERIOR_PADDING).left();
  gfx::Insets insets(padding, padding - location_icon_padding, padding,
                     padding);

  auto layout = std::make_unique<views::FlexLayout>();
  layout->SetOrientation(views::LayoutOrientation::kHorizontal)
      .SetMainAxisAlignment(views::LayoutAlignment::kStart)
      .SetCrossAxisAlignment(views::LayoutAlignment::kCenter)
      .SetInteriorMargin(insets);

  SetLayoutManager(std::move(layout));

  tab_strip_model_observer_.Add(browser->tab_strip_model());
}

CustomTabBarView::~CustomTabBarView() {}

gfx::Rect CustomTabBarView::GetAnchorBoundsInScreen() const {
  return gfx::UnionRects(location_icon_view_->GetAnchorBoundsInScreen(),
                         title_origin_view_->GetAnchorBoundsInScreen());
}

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

gfx::Size CustomTabBarView::CalculatePreferredSize() const {
  // ToolbarView::GetMinimumSize() uses the preferred size of its children, so
  // tell it the minimum size this control will fit into (its layout will
  // automatically have this control fill available space).
  return gfx::Size(GetInsets().width() +
                       title_origin_view_->GetMinimumSize().width() +
                       close_button_->GetPreferredSize().width() +
                       location_icon_view_->GetPreferredSize().width(),
                   GetLayoutManager()->GetPreferredSize(this).height());
}

void CustomTabBarView::OnPaintBackground(gfx::Canvas* canvas) {
  views::View::OnPaintBackground(canvas);

  constexpr SkColor kSeparatorColor = SK_ColorBLACK;
  constexpr float kSeparatorOpacity = 0.15f;

  gfx::Rect bounds = GetLocalBounds();
  // Inset the bounds by 1 on the bottom, so we draw the bottom border inside
  // the custom tab bar.
  bounds.Inset(0, 0, 0, 1);

  // Custom tab/content separator (bottom border).
  canvas->DrawLine(
      bounds.bottom_left(), bounds.bottom_right(),
      color_utils::AlphaBlend(kSeparatorColor, kCustomTabBarViewBackgroundColor,
                              kSeparatorOpacity));

  // Don't render the separator if there is already sufficient contrast between
  // the custom tab bar and the title bar.
  constexpr float kMaxContrastForSeparator = 1.1f;
  if (color_utils::GetContrastRatio(kCustomTabBarViewBackgroundColor,
                                    title_bar_color_) >
      kMaxContrastForSeparator) {
    return;
  }

  // Frame/Custom tab separator (top border).
  canvas->DrawLine(bounds.origin(), bounds.top_right(),
                   color_utils::AlphaBlend(kSeparatorColor, title_bar_color_,
                                           kSeparatorOpacity));
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
  return GetOmniboxSecurityChipColor(OmniboxTint::LIGHT, security_level);
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

void CustomTabBarView::ButtonPressed(views::Button* sender,
                                     const ui::Event& event) {
  GoBackToApp(GetWebContents());
}
