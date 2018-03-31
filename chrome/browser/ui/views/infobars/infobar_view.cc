// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/infobars/infobar_view.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/infobar_container_delegate.h"
#include "chrome/browser/ui/views/harmony/chrome_layout_provider.h"
#include "chrome/browser/ui/views/harmony/chrome_typography.h"
#include "chrome/browser/ui/views/infobars/infobar_background.h"
#include "chrome/grit/generated_resources.h"
#include "components/infobars/core/infobar_delegate.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "third_party/skia/include/effects/SkGradientShader.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/class_property.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/theme_provider.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/native_theme/common_theme.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/button/label_button_border.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/view_properties.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/non_client_view.h"

// Helpers --------------------------------------------------------------------

// Used to mark children that are Labels, so we can update their background
// colors on a theme change.
enum class LabelType {
  kNone,
  kLabel,
  kLink,
};
DEFINE_UI_CLASS_PROPERTY_TYPE(LabelType);

namespace {

DEFINE_UI_CLASS_PROPERTY_KEY(LabelType, kLabelType, LabelType::kNone);

// IDs of the colors to use for infobar elements.
constexpr int kBackgroundColor =
    ThemeProperties::COLOR_DETACHED_BOOKMARK_BAR_BACKGROUND;
constexpr int kTextColor = ThemeProperties::COLOR_BOOKMARK_TEXT;

bool SortLabelsByDecreasingWidth(views::Label* label_1, views::Label* label_2) {
  return label_1->GetPreferredSize().width() >
      label_2->GetPreferredSize().width();
}

int GetElementSpacing() {
  return ChromeLayoutProvider::Get()->GetDistanceMetric(
      DISTANCE_UNRELATED_CONTROL_HORIZONTAL);
}

gfx::Insets GetCloseButtonSpacing() {
  auto* provider = ChromeLayoutProvider::Get();
  const gfx::Insets vector_button_insets =
      provider->GetInsetsMetric(views::INSETS_VECTOR_IMAGE_BUTTON);
  return gfx::Insets(
             provider->GetDistanceMetric(DISTANCE_TOAST_CONTROL_VERTICAL),
             GetElementSpacing()) -
         vector_button_insets;
}

}  // namespace


// InfoBarView ----------------------------------------------------------------

InfoBarView::InfoBarView(std::unique_ptr<infobars::InfoBarDelegate> delegate)
    : infobars::InfoBar(std::move(delegate)),
      views::ExternalFocusTracker(this, nullptr),
      child_container_(new views::View()) {
  set_owned_by_client();  // InfoBar deletes itself at the appropriate time.
  SetBackground(std::make_unique<InfoBarBackground>());
  SetEventTargeter(std::make_unique<views::ViewTargeter>(this));

  AddChildView(child_container_);

  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);

  child_container_->SetPaintToLayer();
  child_container_->layer()->SetMasksToBounds(true);
}

const infobars::InfoBarContainer::Delegate* InfoBarView::container_delegate()
    const {
  const infobars::InfoBarContainer* infobar_container = container();
  return infobar_container ? infobar_container->delegate() : NULL;
}

InfoBarView::~InfoBarView() {
  // We should have closed any open menus in PlatformSpecificHide(), then
  // subclasses' RunMenu() functions should have prevented opening any new ones
  // once we became unowned.
  DCHECK(!menu_runner_.get());
}

views::Label* InfoBarView::CreateLabel(const base::string16& text) const {
  views::Label* label = new views::Label(text, CONTEXT_BODY_TEXT_LARGE);
  SetLabelDetails(label);
  label->SetEnabledColor(GetColor(kTextColor));
  label->SetProperty(kLabelType, LabelType::kLabel);
  return label;
}

views::Link* InfoBarView::CreateLink(const base::string16& text,
                                     views::LinkListener* listener) const {
  views::Link* link = new views::Link(text, CONTEXT_BODY_TEXT_LARGE);
  SetLabelDetails(link);
  link->set_listener(listener);
  link->SetProperty(kLabelType, LabelType::kLink);
  return link;
}

// static
void InfoBarView::AssignWidths(Labels* labels, int available_width) {
  std::sort(labels->begin(), labels->end(), SortLabelsByDecreasingWidth);
  AssignWidthsSorted(labels, available_width);
}

void InfoBarView::Layout() {
  child_container_->SetBounds(
      0, arrow_height(), width(),
      bar_height() - InfoBarContainerDelegate::kSeparatorLineHeight);
  // |child_container_| should be the only child.
  DCHECK_EQ(1, child_count());

  // Even though other views are technically grandchildren, we'll lay them out
  // here on behalf of |child_container_|.
  const int spacing = GetElementSpacing();
  int start_x = 0;
  if (icon_) {
    icon_->SetPosition(gfx::Point(spacing, OffsetY(icon_)));
    start_x = icon_->bounds().right();
  }

  const int content_minimum_width = ContentMinimumWidth();
  if (content_minimum_width > 0)
    start_x += spacing + content_minimum_width;

  const gfx::Insets close_button_spacing = GetCloseButtonSpacing();
  close_button_->SizeToPreferredSize();
  close_button_->SetPosition(gfx::Point(
      std::max(start_x + close_button_spacing.left(),
               width() - close_button_spacing.right() - close_button_->width()),
      OffsetY(close_button_)));

  // For accessibility reasons, the close button should come last.
  DCHECK_EQ(close_button_->parent()->child_count() - 1,
            close_button_->parent()->GetIndexOf(close_button_));
}

void InfoBarView::ViewHierarchyChanged(
    const ViewHierarchyChangedDetails& details) {
  View::ViewHierarchyChanged(details);

  if (details.is_add && (details.child == this) && (close_button_ == NULL)) {
    gfx::Image image = delegate()->GetIcon();
    if (!image.IsEmpty()) {
      icon_ = new views::ImageView;
      icon_->SetImage(image.ToImageSkia());
      icon_->SizeToPreferredSize();
      icon_->SetProperty(
          views::kMarginsKey,
          new gfx::Insets(ChromeLayoutProvider::Get()->GetDistanceMetric(
                              DISTANCE_TOAST_LABEL_VERTICAL),
                          0));
      child_container_->AddChildView(icon_);
    }

    close_button_ = views::CreateVectorImageButton(this);
    views::SetImageFromVectorIcon(close_button_, vector_icons::kClose16Icon,
                                  GetColor(kTextColor));
    close_button_->SetAccessibleName(
        l10n_util::GetStringUTF16(IDS_ACCNAME_CLOSE));
    close_button_->SetFocusForPlatform();
    gfx::Insets close_button_spacing = GetCloseButtonSpacing();
    close_button_->SetProperty(
        views::kMarginsKey, new gfx::Insets(close_button_spacing.top(), 0,
                                            close_button_spacing.bottom(), 0));
    // Subclasses should already be done adding child views by this point (see
    // related DCHECK in Layout()).
    child_container_->AddChildView(close_button_);
  }

  // Ensure the infobar is tall enough to display its contents.
  int height = 0;
  for (int i = 0; i < child_container_->child_count(); ++i) {
    View* child = child_container_->child_at(i);
    const gfx::Insets* const margins = child->GetProperty(views::kMarginsKey);
    const int margin_height = margins ? margins->height() : 0;
    height = std::max(height, child->height() + margin_height);
  }
  SetBarTargetHeight(height + InfoBarContainerDelegate::kSeparatorLineHeight);
}

void InfoBarView::OnThemeChanged() {
  const SkColor background_color = GetColor(kBackgroundColor);
  child_container_->SetBackground(
      views::CreateSolidBackground(background_color));

  const SkColor text_color = GetColor(kTextColor);
  if (close_button_) {
    views::SetImageFromVectorIcon(close_button_, vector_icons::kClose16Icon,
                                  text_color);
  }

  for (int i = 0; i < child_container_->child_count(); ++i) {
    View* child = child_container_->child_at(i);
    LabelType label_type = child->GetProperty(kLabelType);
    if (label_type != LabelType::kNone) {
      auto* label = static_cast<views::Label*>(child);
      label->SetBackgroundColor(background_color);
      if (label_type == LabelType::kLabel)
        label->SetEnabledColor(text_color);
    }
  }
}

void InfoBarView::OnNativeThemeChanged(const ui::NativeTheme* theme) {
  OnThemeChanged();
}

void InfoBarView::ButtonPressed(views::Button* sender,
                                const ui::Event& event) {
  if (!owner())
    return;  // We're closing; don't call anything, it might access the owner.
  if (sender == close_button_) {
    delegate()->InfoBarDismissed();
    RemoveSelf();
  }
}

int InfoBarView::ContentMinimumWidth() const {
  return 0;
}

int InfoBarView::StartX() const {
  // Ensure we don't return a value greater than EndX(), so children can safely
  // set something's width to "EndX() - StartX()" without risking that being
  // negative.
  return std::min((icon_ ? icon_->bounds().right() : 0) + GetElementSpacing(),
                  EndX());
}

int InfoBarView::EndX() const {
  return close_button_->x() - GetCloseButtonSpacing().left();
}

int InfoBarView::OffsetY(views::View* view) const {
  return std::max((bar_target_height() - view->height()) / 2, 0) -
         (bar_target_height() - bar_height());
}

void InfoBarView::AddViewToContentArea(views::View* view) {
  child_container_->AddChildView(view);
}

// static
void InfoBarView::AssignWidthsSorted(Labels* labels, int available_width) {
  if (labels->empty())
    return;
  gfx::Size back_label_size(labels->back()->GetPreferredSize());
  back_label_size.set_width(
      std::min(back_label_size.width(),
               available_width / static_cast<int>(labels->size())));
  labels->back()->SetSize(back_label_size);
  labels->pop_back();
  AssignWidthsSorted(labels, available_width - back_label_size.width());
}

void InfoBarView::PlatformSpecificShow(bool animate) {
  // If we gain focus, we want to restore it to the previously-focused element
  // when we're hidden. So when we're in a Widget, create a focus tracker so
  // that if we gain focus we'll know what the previously-focused element was.
  SetFocusManager(GetFocusManager());

  NotifyAccessibilityEvent(ax::mojom::Event::kAlert, true);
}

void InfoBarView::PlatformSpecificHide(bool animate) {
  // Cancel any menus we may have open.  It doesn't make sense to leave them
  // open while we're hidden, and if we're going to become unowned, we can't
  // allow the user to choose any options and potentially call functions that
  // try to access the owner.
  menu_runner_.reset();

  // It's possible to be called twice (once with |animate| true and once with it
  // false); in this case the second SetFocusManager() call will silently no-op.
  SetFocusManager(NULL);

  if (!animate)
    return;

  // Do not restore focus (and active state with it) if some other top-level
  // window became active.
  views::Widget* widget = GetWidget();
  if (!widget || widget->IsActive())
    FocusLastFocusedExternalView();
}

void InfoBarView::PlatformSpecificOnHeightsRecalculated() {
  // Ensure that notifying our container of our size change will result in a
  // re-layout.
  InvalidateLayout();
}

void InfoBarView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  node_data->SetName(l10n_util::GetStringUTF8(IDS_ACCNAME_INFOBAR));
  node_data->role = ax::mojom::Role::kAlert;
  node_data->AddStringAttribute(ax::mojom::StringAttribute::kKeyShortcuts,
                                "Alt+Shift+A");
}

gfx::Size InfoBarView::CalculatePreferredSize() const {
  int width = 0;

  const int spacing = GetElementSpacing();
  if (icon_)
    width += spacing + icon_->width();

  const int content_width = ContentMinimumWidth();
  if (content_width)
    width += spacing + content_width;

  return gfx::Size(
      width + GetCloseButtonSpacing().width() + close_button_->width(),
      total_height());
}

void InfoBarView::OnWillChangeFocus(View* focused_before, View* focused_now) {
  views::ExternalFocusTracker::OnWillChangeFocus(focused_before, focused_now);

  // This will trigger some screen readers to read the entire contents of this
  // infobar.
  if (focused_before && focused_now && !Contains(focused_before) &&
      Contains(focused_now)) {
    NotifyAccessibilityEvent(ax::mojom::Event::kAlert, true);
  }
}

bool InfoBarView::DoesIntersectRect(const View* target,
                                    const gfx::Rect& rect) const {
  DCHECK_EQ(this, target);
  // Only events that intersect the portion below the arrow are interesting.
  gfx::Rect non_arrow_bounds = GetLocalBounds();
  non_arrow_bounds.Inset(0, arrow_height(), 0, 0);
  return rect.Intersects(non_arrow_bounds);
}

SkColor InfoBarView::GetColor(int id) const {
  const auto* theme_provider = GetThemeProvider();
  // When there's no theme provider, this color will never be used; it will be
  // reset due to the OnNativeThemeChanged() override.
  return theme_provider ? theme_provider->GetColor(id) : gfx::kPlaceholderColor;
}

void InfoBarView::SetLabelDetails(views::Label* label) const {
  label->SizeToPreferredSize();
  label->SetBackgroundColor(GetColor(kBackgroundColor));
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label->SetProperty(
      views::kMarginsKey,
      new gfx::Insets(ChromeLayoutProvider::Get()->GetDistanceMetric(
                          DISTANCE_TOAST_LABEL_VERTICAL),
                      0));
}
