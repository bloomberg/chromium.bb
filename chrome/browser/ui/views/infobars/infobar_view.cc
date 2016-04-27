// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/infobars/infobar_view.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/infobar_container_delegate.h"
#include "chrome/browser/ui/views/bar_control_button.h"
#include "chrome/browser/ui/views/infobars/infobar_background.h"
#include "chrome/grit/generated_resources.h"
#include "components/infobars/core/infobar_delegate.h"
#include "grit/components_strings.h"
#include "grit/theme_resources.h"
#include "third_party/skia/include/effects/SkGradientShader.h"
#include "ui/accessibility/ax_view_state.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icons_public.h"
#include "ui/native_theme/common_theme.h"
#include "ui/native_theme/native_theme.h"
#include "ui/resources/grit/ui_resources.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/label_button_border.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/layout/layout_constants.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/non_client_view.h"


// Helpers --------------------------------------------------------------------

namespace {

const int kEdgeItemPadding = views::kRelatedControlHorizontalSpacing;
const int kIconToLabelSpacing = views::kRelatedControlHorizontalSpacing;
const int kBeforeCloseButtonSpacing = views::kUnrelatedControlHorizontalSpacing;

bool SortLabelsByDecreasingWidth(views::Label* label_1, views::Label* label_2) {
  return label_1->GetPreferredSize().width() >
      label_2->GetPreferredSize().width();
}

const gfx::FontList& GetFontList() {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  return rb.GetFontList(ui::MaterialDesignController::IsModeMaterial()
                            ? ui::ResourceBundle::BaseFont
                            : ui::ResourceBundle::MediumFont);
}

SkColor GetInfobarTextColor() {
  return SK_ColorBLACK;
}

}  // namespace


// InfoBarView ----------------------------------------------------------------

// static
const int InfoBarView::kButtonButtonSpacing = views::kRelatedButtonHSpacing;
const int InfoBarView::kEndOfLabelSpacing = views::kItemLabelSpacing;

InfoBarView::InfoBarView(std::unique_ptr<infobars::InfoBarDelegate> delegate)
    : infobars::InfoBar(std::move(delegate)),
      views::ExternalFocusTracker(this, nullptr),
      child_container_(new views::View()),
      icon_(nullptr),
      close_button_(nullptr) {
  set_owned_by_client();  // InfoBar deletes itself at the appropriate time.
  set_background(
      new InfoBarBackground(infobars::InfoBar::delegate()->GetInfoBarType()));
  SetEventTargeter(base::WrapUnique(new views::ViewTargeter(this)));

  AddChildView(child_container_);

  SetPaintToLayer(true);
  layer()->SetFillsBoundsOpaquely(false);

  if (ui::MaterialDesignController::IsModeMaterial()) {
    child_container_->SetPaintToLayer(true);
    child_container_->layer()->SetMasksToBounds(true);
    // Since MD doesn't use a gradient, we can set a solid bg color.
    child_container_->set_background(
        views::Background::CreateSolidBackground(infobars::InfoBar::GetTopColor(
            infobars::InfoBar::delegate()->GetInfoBarType())));
  }
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
  views::Label* label = new views::Label(text, GetFontList());
  label->SizeToPreferredSize();
  label->SetBackgroundColor(background()->get_color());
  label->SetEnabledColor(GetInfobarTextColor());
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  return label;
}

views::Link* InfoBarView::CreateLink(const base::string16& text,
                                     views::LinkListener* listener) const {
  views::Link* link = new views::Link(text);
  link->SetFontList(GetFontList());
  link->SizeToPreferredSize();
  link->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  link->set_listener(listener);
  link->SetBackgroundColor(background()->get_color());
  return link;
}

// static
views::LabelButton* InfoBarView::CreateTextButton(
    views::ButtonListener* listener,
    const base::string16& text) {
  DCHECK(!ui::MaterialDesignController::IsModeMaterial());
  views::LabelButton* button = new views::LabelButton(listener, text);
  std::unique_ptr<views::LabelButtonAssetBorder> button_border(
      new views::LabelButtonAssetBorder(views::Button::STYLE_TEXTBUTTON));
  const int kNormalImageSet[] = IMAGE_GRID(IDR_INFOBARBUTTON_NORMAL);
  button_border->SetPainter(
      false, views::Button::STATE_NORMAL,
      views::Painter::CreateImageGridPainter(kNormalImageSet));
  const int kHoveredImageSet[] = IMAGE_GRID(IDR_INFOBARBUTTON_HOVER);
  button_border->SetPainter(
      false, views::Button::STATE_HOVERED,
      views::Painter::CreateImageGridPainter(kHoveredImageSet));
  const int kPressedImageSet[] = IMAGE_GRID(IDR_INFOBARBUTTON_PRESSED);
  button_border->SetPainter(
      false, views::Button::STATE_PRESSED,
      views::Painter::CreateImageGridPainter(kPressedImageSet));
  button->SetBorder(std::move(button_border));
  button->set_animate_on_state_change(false);
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  button->SetFontList(rb.GetFontList(ui::ResourceBundle::MediumFont));
  button->SetFocusBehavior(FocusBehavior::ALWAYS);
  button->SetTextColor(views::Button::STATE_NORMAL, GetInfobarTextColor());
  button->SetTextColor(views::Button::STATE_HOVERED, GetInfobarTextColor());
  return button;
}

views::MdTextButton* InfoBarView::CreateMdTextButton(
    views::ButtonListener* listener,
    const base::string16& text) {
  DCHECK(ui::MaterialDesignController::IsModeMaterial());
  views::MdTextButton* button =
      views::MdTextButton::CreateMdButton(listener, text);
  button->SetTextColor(views::Button::STATE_NORMAL, GetInfobarTextColor());
  button->SetTextColor(views::Button::STATE_HOVERED, GetInfobarTextColor());
  return button;
}

// static
void InfoBarView::AssignWidths(Labels* labels, int available_width) {
  std::sort(labels->begin(), labels->end(), SortLabelsByDecreasingWidth);
  AssignWidthsSorted(labels, available_width);
}

void InfoBarView::Layout() {
  // Calculate the fill and stroke paths.  We do this here, rather than in
  // PlatformSpecificRecalculateHeight(), because this is also reached when our
  // width is changed, which affects both paths.
  // TODO(estade): these paths aren't used for MD; remove when MD is default.
  stroke_path_.rewind();
  fill_path_.rewind();
  const infobars::InfoBarContainer::Delegate* delegate = container_delegate();
  if (delegate) {
    int arrow_x;
    SkScalar arrow_fill_height = SkIntToScalar(std::max(
        arrow_height() - InfoBarContainerDelegate::kSeparatorLineHeight, 0));
    SkScalar arrow_fill_half_width = SkIntToScalar(arrow_half_width());
    SkScalar separator_height =
        SkIntToScalar(InfoBarContainerDelegate::kSeparatorLineHeight);
    if (delegate->DrawInfoBarArrows(&arrow_x) && arrow_fill_height) {
      // Skia pixel centers are at the half-values, so the arrow is horizontally
      // centered at |arrow_x| + 0.5.  Vertically, the stroke path is the center
      // of the separator, while the fill path is a closed path that extends up
      // through the entire height of the separator and down to the bottom of
      // the arrow where it joins the bar.
      stroke_path_.moveTo(
          SkIntToScalar(arrow_x) + SK_ScalarHalf - arrow_fill_half_width,
          SkIntToScalar(arrow_height()) - (separator_height * SK_ScalarHalf));
      stroke_path_.rLineTo(arrow_fill_half_width, -arrow_fill_height);
      stroke_path_.rLineTo(arrow_fill_half_width, arrow_fill_height);

      fill_path_ = stroke_path_;
      // Move the top of the fill path up to the top of the separator and then
      // extend it down all the way through.
      fill_path_.offset(0, -separator_height * SK_ScalarHalf);
      // This 0.01 hack prevents the fill from filling more pixels on the right
      // edge of the arrow than on the left.
      const SkScalar epsilon = 0.01f;
      fill_path_.rLineTo(-epsilon, 0);
      fill_path_.rLineTo(0, separator_height);
      fill_path_.rLineTo(epsilon - (arrow_fill_half_width * 2), 0);
      fill_path_.close();
    }
  }
  if (bar_height()) {
    fill_path_.addRect(
        0.0, SkIntToScalar(arrow_height()), SkIntToScalar(width()),
        SkIntToScalar(
            height() - InfoBarContainerDelegate::kSeparatorLineHeight));
  }

  child_container_->SetBounds(
      0, arrow_height(), width(),
      bar_height() - InfoBarContainerDelegate::kSeparatorLineHeight);
  // |child_container_| should be the only child.
  DCHECK_EQ(1, child_count());

  // Even though other views are technically grandchildren, we'll lay them out
  // here on behalf of |child_container_|.
  int start_x = kEdgeItemPadding;
  if (icon_ != NULL) {
    icon_->SetPosition(gfx::Point(start_x, OffsetY(icon_)));
    start_x = icon_->bounds().right() + kIconToLabelSpacing;
  }

  int content_minimum_width = ContentMinimumWidth();
  close_button_->SizeToPreferredSize();
  close_button_->SetPosition(gfx::Point(
      std::max(
          start_x + content_minimum_width +
              ((content_minimum_width > 0) ? kBeforeCloseButtonSpacing : 0),
          width() - kEdgeItemPadding - close_button_->width()),
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
      child_container_->AddChildView(icon_);
    }

    if (ui::MaterialDesignController::IsModeMaterial()) {
      BarControlButton* close = new BarControlButton(this);
      close->SetIcon(gfx::VectorIconId::BAR_CLOSE,
                     base::Bind(&GetInfobarTextColor));
      close->set_request_focus_on_press(false);
      close_button_ = close;
    } else {
      close_button_ = new views::ImageButton(this);
      ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
      close_button_->SetImage(views::CustomButton::STATE_NORMAL,
                              rb.GetImageNamed(IDR_CLOSE_1).ToImageSkia());
      close_button_->SetImage(views::CustomButton::STATE_HOVERED,
                              rb.GetImageNamed(IDR_CLOSE_1_H).ToImageSkia());
      close_button_->SetImage(views::CustomButton::STATE_PRESSED,
                              rb.GetImageNamed(IDR_CLOSE_1_P).ToImageSkia());
    }
    close_button_->SetAccessibleName(
        l10n_util::GetStringUTF16(IDS_ACCNAME_CLOSE));
    close_button_->SetFocusBehavior(FocusBehavior::ALWAYS);
    // Subclasses should already be done adding child views by this point (see
    // related DCHECK in Layout()).
    child_container_->AddChildView(close_button_);
  }

  // Ensure the infobar is tall enough to display its contents.
  int height = ui::MaterialDesignController::IsModeMaterial()
                   ? InfoBarContainerDelegate::kDefaultBarTargetHeightMd
                   : InfoBarContainerDelegate::kDefaultBarTargetHeight;
  const int kMinimumVerticalPadding = 6;
  for (int i = 0; i < child_container_->child_count(); ++i) {
    const int child_height = child_container_->child_at(i)->height();
    height = std::max(height, child_height + kMinimumVerticalPadding);
  }
  SetBarTargetHeight(height);
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
  return std::min(EndX(), (icon_ != NULL) ?
      (icon_->bounds().right() + kIconToLabelSpacing) : kEdgeItemPadding);
}

int InfoBarView::EndX() const {
  return close_button_->x() - kBeforeCloseButtonSpacing;
}

int InfoBarView::OffsetY(views::View* view) const {
  return std::max((bar_target_height() - view->height()) / 2, 0) -
         (bar_target_height() - bar_height());
}

void InfoBarView::RunMenuAt(ui::MenuModel* menu_model,
                            views::MenuButton* button,
                            views::MenuAnchorPosition anchor) {
  DCHECK(owner());  // We'd better not open any menus while we're closing.
  gfx::Point screen_point;
  views::View::ConvertPointToScreen(button, &screen_point);
  menu_runner_.reset(
      new views::MenuRunner(menu_model, views::MenuRunner::HAS_MNEMONICS));
  // Ignore the result since we don't need to handle a deleted menu specially.
  ignore_result(menu_runner_->RunMenuAt(GetWidget(),
                                        button,
                                        gfx::Rect(screen_point, button->size()),
                                        anchor,
                                        ui::MENU_SOURCE_NONE));
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

  NotifyAccessibilityEvent(ui::AX_EVENT_ALERT, true);
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

void InfoBarView::GetAccessibleState(ui::AXViewState* state) {
  state->name = l10n_util::GetStringUTF16(
      (delegate()->GetInfoBarType() ==
       infobars::InfoBarDelegate::WARNING_TYPE) ?
          IDS_ACCNAME_INFOBAR_WARNING : IDS_ACCNAME_INFOBAR_PAGE_ACTION);
  state->role = ui::AX_ROLE_ALERT;
  state->keyboard_shortcut = base::ASCIIToUTF16("Alt+Shift+A");
}

gfx::Size InfoBarView::GetPreferredSize() const {
  return gfx::Size(
      kEdgeItemPadding + (icon_ ? (icon_->width() + kIconToLabelSpacing) : 0) +
          ContentMinimumWidth() + kBeforeCloseButtonSpacing +
          close_button_->width() + kEdgeItemPadding,
      total_height());
}

void InfoBarView::OnWillChangeFocus(View* focused_before, View* focused_now) {
  views::ExternalFocusTracker::OnWillChangeFocus(focused_before, focused_now);

  // This will trigger some screen readers to read the entire contents of this
  // infobar.
  if (focused_before && focused_now && !Contains(focused_before) &&
      Contains(focused_now)) {
    NotifyAccessibilityEvent(ui::AX_EVENT_ALERT, true);
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
