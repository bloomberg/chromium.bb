// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/system/tray/tray_popup_utils.h"

#include "ash/common/ash_constants.h"
#include "ash/common/ash_view_ids.h"
#include "ash/common/material_design/material_design_controller.h"
#include "ash/common/session/session_state_delegate.h"
#include "ash/common/system/tray/fixed_sized_image_view.h"
#include "ash/common/system/tray/size_range_layout.h"
#include "ash/common/system/tray/tray_constants.h"
#include "ash/common/system/tray/tray_popup_item_style.h"
#include "ash/common/system/tray/tray_popup_label_button.h"
#include "ash/common/system/tray/tray_popup_label_button_border.h"
#include "ash/common/wm_shell.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/animation/flood_fill_ink_drop_ripple.h"
#include "ui/views/animation/ink_drop_highlight.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/animation/ink_drop_mask.h"
#include "ui/views/animation/square_ink_drop_ripple.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/button/toggle_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/separator.h"
#include "ui/views/controls/slider.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/painter.h"

namespace ash {

namespace {

// Creates a layout manager that positions Views vertically. The Views will be
// stretched horizontally and centered vertically.
std::unique_ptr<views::LayoutManager> CreateDefaultCenterLayoutManager() {
  // TODO(bruthig): Use constants instead of magic numbers.
  auto box_layout =
      base::MakeUnique<views::BoxLayout>(views::BoxLayout::kVertical, 4, 8, 0);
  box_layout->set_main_axis_alignment(
      views::BoxLayout::MAIN_AXIS_ALIGNMENT_CENTER);
  box_layout->set_cross_axis_alignment(
      views::BoxLayout::CROSS_AXIS_ALIGNMENT_STRETCH);
  return std::move(box_layout);
}

// Creates a layout manager that positions Views horizontally. The Views will be
// centered along the horizontal and vertical axis.
std::unique_ptr<views::LayoutManager> CreateDefaultEndsLayoutManager() {
  auto box_layout = base::MakeUnique<views::BoxLayout>(
      views::BoxLayout::kHorizontal, 0, 0, 0);
  box_layout->set_main_axis_alignment(
      views::BoxLayout::MAIN_AXIS_ALIGNMENT_CENTER);
  box_layout->set_cross_axis_alignment(
      views::BoxLayout::CROSS_AXIS_ALIGNMENT_CENTER);
  return std::move(box_layout);
}

std::unique_ptr<views::LayoutManager> CreateDefaultLayoutManager(
    TriView::Container container) {
  switch (container) {
    case TriView::Container::START:
    case TriView::Container::END:
      return CreateDefaultEndsLayoutManager();
    case TriView::Container::CENTER:
      return CreateDefaultCenterLayoutManager();
  }
  // Required by some compilers.
  NOTREACHED();
  return nullptr;
}

// Configures the default size and flex value for the specified |container|
// of the given |tri_view|. Used by CreateDefaultRowView().
void ConfigureDefaultSizeAndFlex(TriView* tri_view,
                                 TriView::Container container) {
  int min_width = 0;
  switch (container) {
    case TriView::Container::START:
      min_width = GetTrayConstant(TRAY_POPUP_ITEM_MIN_START_WIDTH);
      break;
    case TriView::Container::CENTER:
      tri_view->SetFlexForContainer(TriView::Container::CENTER, 1.f);
      break;
    case TriView::Container::END:
      min_width = GetTrayConstant(TRAY_POPUP_ITEM_MIN_END_WIDTH);
      break;
  }

  tri_view->SetMinSize(
      container,
      gfx::Size(min_width, GetTrayConstant(TRAY_POPUP_ITEM_MIN_HEIGHT)));
  tri_view->SetMaxSize(container,
                       gfx::Size(SizeRangeLayout::kAbsoluteMaxSize,
                                 GetTrayConstant(TRAY_POPUP_ITEM_MAX_HEIGHT)));
}

class BorderlessLabelButton : public views::LabelButton {
 public:
  BorderlessLabelButton(views::ButtonListener* listener,
                        const base::string16& text)
      : LabelButton(listener, text) {
    if (MaterialDesignController::IsSystemTrayMenuMaterial()) {
      SetInkDropMode(views::InkDropHostView::InkDropMode::ON);
      set_has_ink_drop_action_on_click(true);
      set_ink_drop_base_color(kTrayPopupInkDropBaseColor);
      set_ink_drop_visible_opacity(kTrayPopupInkDropRippleOpacity);
      const int kHorizontalPadding = 20;
      SetBorder(views::CreateEmptyBorder(gfx::Insets(0, kHorizontalPadding)));
      TrayPopupItemStyle style(TrayPopupItemStyle::FontStyle::BUTTON);
      style.SetupLabel(label());
      // TODO(tdanderson): Update focus rect for material design. See
      // crbug.com/615892
      // Hack alert: CreateSolidFocusPainter should add 0.5f to all insets to
      // make the lines align to pixel centers, but for now it doesn't. We can
      // get around this by relying on Skia rounding up integer coordinates.
      SetFocusPainter(views::Painter::CreateSolidFocusPainter(
          kFocusBorderColor, gfx::Insets(0, 0, 1, 1)));
    } else {
      SetBorder(std::unique_ptr<views::Border>(new TrayPopupLabelButtonBorder));
      SetFocusPainter(views::Painter::CreateSolidFocusPainter(
          kFocusBorderColor, gfx::Insets(1, 1, 2, 2)));
      set_animate_on_state_change(false);
    }
    SetHorizontalAlignment(gfx::ALIGN_CENTER);
    SetFocusForPlatform();
  }

  ~BorderlessLabelButton() override {}

  // views::LabelButton:
  int GetHeightForWidth(int width) const override {
    if (MaterialDesignController::IsSystemTrayMenuMaterial())
      return kMenuButtonSize;

    return LabelButton::GetHeightForWidth(width);
  }

 private:
  std::unique_ptr<views::InkDrop> CreateInkDrop() override {
    return TrayPopupUtils::CreateInkDrop(TrayPopupInkDropStyle::INSET_BOUNDS,
                                         this);
  }

  std::unique_ptr<views::InkDropRipple> CreateInkDropRipple() const override {
    return TrayPopupUtils::CreateInkDropRipple(
        TrayPopupInkDropStyle::INSET_BOUNDS, this,
        GetInkDropCenterBasedOnLastEvent());
  }

  std::unique_ptr<views::InkDropHighlight> CreateInkDropHighlight()
      const override {
    return TrayPopupUtils::CreateInkDropHighlight(
        TrayPopupInkDropStyle::INSET_BOUNDS, this);
  }

  std::unique_ptr<views::InkDropMask> CreateInkDropMask() const override {
    return TrayPopupUtils::CreateInkDropMask(
        TrayPopupInkDropStyle::INSET_BOUNDS, this);
  }

  DISALLOW_COPY_AND_ASSIGN(BorderlessLabelButton);
};

}  // namespace

TriView* TrayPopupUtils::CreateDefaultRowView() {
  TriView* tri_view = CreateMultiTargetRowView();

  tri_view->SetContainerLayout(
      TriView::Container::START,
      CreateDefaultLayoutManager(TriView::Container::START));
  tri_view->SetContainerLayout(
      TriView::Container::CENTER,
      CreateDefaultLayoutManager(TriView::Container::CENTER));
  tri_view->SetContainerLayout(
      TriView::Container::END,
      CreateDefaultLayoutManager(TriView::Container::END));

  return tri_view;
}

TriView* TrayPopupUtils::CreateSubHeaderRowView() {
  TriView* tri_view = CreateMultiTargetRowView();
  tri_view->SetInsets(
      gfx::Insets(0, kTrayPopupPaddingHorizontal, 0,
                  GetTrayConstant(TRAY_POPUP_ITEM_RIGHT_INSET)));
  tri_view->SetContainerVisible(TriView::Container::START, false);
  tri_view->SetContainerLayout(
      TriView::Container::END,
      CreateDefaultLayoutManager(TriView::Container::END));
  return tri_view;
}

TriView* TrayPopupUtils::CreateMultiTargetRowView() {
  TriView* tri_view = new TriView(0 /* padding_between_items */);

  tri_view->SetInsets(
      gfx::Insets(0, GetTrayConstant(TRAY_POPUP_ITEM_LEFT_INSET), 0,
                  GetTrayConstant(TRAY_POPUP_ITEM_RIGHT_INSET)));

  ConfigureDefaultSizeAndFlex(tri_view, TriView::Container::START);
  ConfigureDefaultSizeAndFlex(tri_view, TriView::Container::CENTER);
  ConfigureDefaultSizeAndFlex(tri_view, TriView::Container::END);

  tri_view->SetContainerLayout(TriView::Container::START,
                               base::MakeUnique<views::FillLayout>());
  tri_view->SetContainerLayout(TriView::Container::CENTER,
                               base::MakeUnique<views::FillLayout>());
  tri_view->SetContainerLayout(TriView::Container::END,
                               base::MakeUnique<views::FillLayout>());

  return tri_view;
}

views::Label* TrayPopupUtils::CreateDefaultLabel() {
  views::Label* label = new views::Label();
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label->SetBorder(
      views::CreateEmptyBorder(0, 0, 0, kTrayPopupLabelRightPadding));

  // TODO(bruthig): Fix this so that |label| uses the kBackgroundColor to
  // perform subpixel rendering instead of disabling subpixel rendering.
  //
  // Text rendered on a non-opaque background looks ugly and it is possible for
  // labels to given a a clear canvas at paint time when an ink drop is visible.
  // See http://crbug.com/661714.
  label->SetSubpixelRenderingEnabled(false);

  return label;
}

views::ImageView* TrayPopupUtils::CreateMainImageView() {
  return new FixedSizedImageView(
      GetTrayConstant(TRAY_POPUP_ITEM_MAIN_IMAGE_CONTAINER_WIDTH),
      GetTrayConstant(TRAY_POPUP_ITEM_MIN_HEIGHT));
}

views::ImageView* TrayPopupUtils::CreateMoreImageView() {
  views::ImageView* image =
      new FixedSizedImageView(GetTrayConstant(TRAY_POPUP_ITEM_MORE_IMAGE_SIZE),
                              GetTrayConstant(TRAY_POPUP_ITEM_MORE_IMAGE_SIZE));
  image->EnableCanvasFlippingForRTLUI(true);
  return image;
}

views::Slider* TrayPopupUtils::CreateSlider(views::SliderListener* listener) {
  const bool is_material = MaterialDesignController::IsSystemTrayMenuMaterial();
  views::Slider* slider = views::Slider::CreateSlider(is_material, listener);
  slider->set_focus_border_color(kFocusBorderColor);
  if (is_material) {
    slider->SetBorder(
        views::CreateEmptyBorder(gfx::Insets(0, kTrayPopupSliderPaddingMD)));
  } else {
    slider->SetBorder(
        views::CreateEmptyBorder(0, 0, 0, kTrayPopupPaddingBetweenItems));
  }
  return slider;
}

views::ToggleButton* TrayPopupUtils::CreateToggleButton(
    views::ButtonListener* listener,
    int accessible_name_id) {
  views::ToggleButton* toggle = new views::ToggleButton(listener);
  const gfx::Size toggle_size(toggle->GetPreferredSize());
  const int vertical_padding = (kMenuButtonSize - toggle_size.height()) / 2;
  const int horizontal_padding =
      (kTrayToggleButtonWidth - toggle_size.width()) / 2;
  toggle->SetBorder(views::CreateEmptyBorder(
      gfx::Insets(vertical_padding, horizontal_padding)));
  // TODO(tdanderson): Update the focus rect color, border thickness, and
  // location for material design.
  toggle->SetFocusPainter(views::Painter::CreateSolidFocusPainter(
      kFocusBorderColor, gfx::Insets(1)));
  toggle->SetAccessibleName(l10n_util::GetStringUTF16(accessible_name_id));
  return toggle;
}

void TrayPopupUtils::ConfigureAsStickyHeader(views::View* view) {
  view->set_id(VIEW_ID_STICKY_HEADER);
  view->set_background(
      views::Background::CreateSolidBackground(kBackgroundColor));
  view->SetBorder(
      views::CreateEmptyBorder(gfx::Insets(kMenuSeparatorVerticalPadding, 0)));
}

void TrayPopupUtils::ConfigureContainer(TriView::Container container,
                                        views::View* container_view) {
  container_view->SetLayoutManager(
      CreateDefaultLayoutManager(container).release());
}

views::LabelButton* TrayPopupUtils::CreateTrayPopupBorderlessButton(
    views::ButtonListener* listener,
    const base::string16& text) {
  return new BorderlessLabelButton(listener, text);
}

views::LabelButton* TrayPopupUtils::CreateTrayPopupButton(
    views::ButtonListener* listener,
    const base::string16& text) {
  if (!MaterialDesignController::IsSystemTrayMenuMaterial())
    return CreateTrayPopupBorderlessButton(listener, text);

  auto* button = views::MdTextButton::Create(listener, text);
  button->SetProminent(true);
  return button;
}

views::Separator* TrayPopupUtils::CreateVerticalSeparator() {
  views::Separator* separator =
      new views::Separator(views::Separator::HORIZONTAL);
  separator->SetPreferredSize(kHorizontalSeparatorHeight);
  separator->SetColor(kHorizontalSeparatorColor);
  return separator;
}

std::unique_ptr<views::InkDrop> TrayPopupUtils::CreateInkDrop(
    TrayPopupInkDropStyle ink_drop_style,
    views::InkDropHostView* host) {
  std::unique_ptr<views::InkDropImpl> ink_drop =
      base::MakeUnique<views::InkDropImpl>(host, host->size());
  ink_drop->SetAutoHighlightMode(
      views::InkDropImpl::AutoHighlightMode::SHOW_ON_RIPPLE);
  ink_drop->SetShowHighlightOnHover(false);

  return std::move(ink_drop);
}

std::unique_ptr<views::InkDropRipple> TrayPopupUtils::CreateInkDropRipple(
    TrayPopupInkDropStyle ink_drop_style,
    const views::View* host,
    const gfx::Point& center_point,
    SkColor color) {
  const gfx::Rect bounds =
      TrayPopupUtils::GetInkDropBounds(ink_drop_style, host);
  switch (ink_drop_style) {
    case TrayPopupInkDropStyle::HOST_CENTERED:
      if (MaterialDesignController::GetMode() ==
          MaterialDesignController::MATERIAL_EXPERIMENTAL) {
        return base::MakeUnique<views::SquareInkDropRipple>(
            bounds.size(), bounds.size().width() / 2, bounds.size(),
            bounds.size().width() / 2, center_point, bounds.CenterPoint(),
            color, kTrayPopupInkDropRippleOpacity);
      }
    // Intentional fall through.
    case TrayPopupInkDropStyle::INSET_BOUNDS:
    case TrayPopupInkDropStyle::FILL_BOUNDS:
      return base::MakeUnique<views::FloodFillInkDropRipple>(
          bounds, center_point, color, kTrayPopupInkDropRippleOpacity);
  }
  // Required for some compilers.
  NOTREACHED();
  return nullptr;
}

std::unique_ptr<views::InkDropHighlight> TrayPopupUtils::CreateInkDropHighlight(
    TrayPopupInkDropStyle ink_drop_style,
    const views::View* host,
    SkColor color) {
  const gfx::Rect bounds =
      TrayPopupUtils::GetInkDropBounds(ink_drop_style, host);
  std::unique_ptr<views::InkDropHighlight> highlight(
      new views::InkDropHighlight(bounds.size(), 0,
                                  gfx::PointF(bounds.CenterPoint()), color));
  highlight->set_visible_opacity(kTrayPopupInkDropHighlightOpacity);
  return highlight;
}

std::unique_ptr<views::InkDropMask> TrayPopupUtils::CreateInkDropMask(
    TrayPopupInkDropStyle ink_drop_style,
    const views::View* host) {
  if (ink_drop_style == TrayPopupInkDropStyle::FILL_BOUNDS)
    return nullptr;

  const gfx::Size layer_size = host->size();
  switch (ink_drop_style) {
    case TrayPopupInkDropStyle::HOST_CENTERED: {
      const gfx::Rect mask_bounds =
          GetInkDropBounds(TrayPopupInkDropStyle::HOST_CENTERED, host);
      const int radius =
          std::min(mask_bounds.width(), mask_bounds.height()) / 2;
      return base::MakeUnique<views::CircleInkDropMask>(
          layer_size, mask_bounds.CenterPoint(), radius);
    }
    case TrayPopupInkDropStyle::INSET_BOUNDS: {
      const gfx::Insets mask_insets =
          GetInkDropInsets(TrayPopupInkDropStyle::INSET_BOUNDS);
      return base::MakeUnique<views::RoundRectInkDropMask>(
          layer_size, mask_insets, kTrayPopupInkDropCornerRadius);
    }
    case TrayPopupInkDropStyle::FILL_BOUNDS:
      // Handled by quick return above.
      break;
  }
  // Required by some compilers.
  NOTREACHED();
  return nullptr;
}

gfx::Insets TrayPopupUtils::GetInkDropInsets(
    TrayPopupInkDropStyle ink_drop_style) {
  gfx::Insets insets;
  if (ink_drop_style == TrayPopupInkDropStyle::HOST_CENTERED ||
      ink_drop_style == TrayPopupInkDropStyle::INSET_BOUNDS) {
    insets.Set(kTrayPopupInkDropInset, kTrayPopupInkDropInset,
               kTrayPopupInkDropInset, kTrayPopupInkDropInset);
  }
  return insets;
}

gfx::Rect TrayPopupUtils::GetInkDropBounds(TrayPopupInkDropStyle ink_drop_style,
                                           const views::View* host) {
  gfx::Rect bounds = host->GetLocalBounds();
  bounds.Inset(GetInkDropInsets(ink_drop_style));
  return bounds;
}

views::Separator* TrayPopupUtils::CreateListItemSeparator(bool left_inset) {
  views::Separator* separator =
      new views::Separator(views::Separator::HORIZONTAL);
  separator->SetColor(kHorizontalSeparatorColor);
  separator->SetPreferredSize(kSeparatorWidth);
  separator->SetBorder(views::CreateEmptyBorder(
      kMenuSeparatorVerticalPadding - kSeparatorWidth,
      left_inset
          ? kMenuExtraMarginFromLeftEdge + kMenuButtonSize +
                kTrayPopupLabelHorizontalPadding
          : 0,
      kMenuSeparatorVerticalPadding, 0));
  return separator;
}

views::Separator* TrayPopupUtils::CreateListSubHeaderSeparator() {
  views::Separator* separator =
      new views::Separator(views::Separator::HORIZONTAL);
  separator->SetColor(kHorizontalSeparatorColor);
  separator->SetPreferredSize(kSeparatorWidth);
  separator->SetBorder(views::CreateEmptyBorder(
      kMenuSeparatorVerticalPadding - kSeparatorWidth, 0, 0, 0));
  return separator;
}

bool TrayPopupUtils::CanOpenWebUISettings(LoginStatus status) {
  // TODO(tdanderson): Consider moving this into WmShell, or introduce a
  // CanShowSettings() method in each delegate type that has a
  // ShowSettings() method.
  return status != LoginStatus::NOT_LOGGED_IN &&
         status != LoginStatus::LOCKED &&
         !WmShell::Get()->GetSessionStateDelegate()->IsInSecondaryLoginScreen();
}

}  // namespace ash
