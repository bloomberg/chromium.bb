// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/system/tray/tray_popup_utils.h"

#include "ash/common/ash_constants.h"
#include "ash/common/material_design/material_design_controller.h"
#include "ash/common/session/session_state_delegate.h"
#include "ash/common/system/tray/fixed_sized_image_view.h"
#include "ash/common/system/tray/tray_constants.h"
#include "ash/common/system/tray/tray_popup_label_button.h"
#include "ash/common/system/tray/tray_popup_label_button_border.h"
#include "ash/common/wm_shell.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/separator.h"
#include "ui/views/controls/slider.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"

namespace ash {

namespace {

// Creates a layout manager that positions Views vertically. The Views will be
// stretched horizontally and centered vertically.
std::unique_ptr<views::LayoutManager> CreateDefaultCenterLayoutManager() {
  // TODO(bruthig): Use constants instead of magic numbers.
  views::BoxLayout* box_layout =
      new views::BoxLayout(views::BoxLayout::kVertical, 4, 8, 4);
  box_layout->set_main_axis_alignment(
      views::BoxLayout::MAIN_AXIS_ALIGNMENT_CENTER);
  box_layout->set_cross_axis_alignment(
      views::BoxLayout::CROSS_AXIS_ALIGNMENT_STRETCH);
  return std::unique_ptr<views::LayoutManager>(box_layout);
}

// Creates a layout manager that positions Views horizontally. The Views will be
// centered along the horizontal and vertical axis.
std::unique_ptr<views::LayoutManager> CreateDefaultEndsLayoutManager() {
  views::BoxLayout* box_layout =
      new views::BoxLayout(views::BoxLayout::kHorizontal, 0, 0, 0);
  box_layout->set_main_axis_alignment(
      views::BoxLayout::MAIN_AXIS_ALIGNMENT_CENTER);
  box_layout->set_cross_axis_alignment(
      views::BoxLayout::CROSS_AXIS_ALIGNMENT_STRETCH);
  return std::unique_ptr<views::LayoutManager>(box_layout);
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
      // TODO(tdanderson): Update focus rect for material design. See
      // crbug.com/615892
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
      return kMenuButtonSize - 2 * kTrayPopupInkDropInset;

    return LabelButton::GetHeightForWidth(width);
  }

 private:
  std::unique_ptr<views::InkDrop> CreateInkDrop() override {
    std::unique_ptr<views::InkDropImpl> ink_drop =
        CreateDefaultFloodFillInkDropImpl();
    ink_drop->SetShowHighlightOnHover(false);
    return std::move(ink_drop);
  }

  DISALLOW_COPY_AND_ASSIGN(BorderlessLabelButton);
};

}  // namespace

TriView* TrayPopupUtils::CreateDefaultRowView() {
  TriView* tri_view = CreateMultiTargetRowView();
  tri_view->SetContainerBorder(TriView::Container::START,
                               CreateDefaultBorder(TriView::Container::START));
  tri_view->SetContainerBorder(TriView::Container::CENTER,
                               CreateDefaultBorder(TriView::Container::CENTER));
  tri_view->SetContainerBorder(TriView::Container::END,
                               CreateDefaultBorder(TriView::Container::END));

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

TriView* TrayPopupUtils::CreateMultiTargetRowView() {
  TriView* tri_view = new TriView(0 /* padding_between_items */);

  tri_view->SetInsets(
      gfx::Insets(0, GetTrayConstant(TRAY_POPUP_ITEM_LEFT_INSET), 0,
                  GetTrayConstant(TRAY_POPUP_ITEM_RIGHT_INSET)));
  tri_view->SetMinCrossAxisSize(GetTrayConstant(TRAY_POPUP_ITEM_HEIGHT));

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
  label->SetBorder(views::CreateEmptyBorder(0, kTrayPopupLabelHorizontalPadding,
                                            0,
                                            kTrayPopupLabelHorizontalPadding));

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
      GetTrayConstant(TRAY_POPUP_ITEM_HEIGHT));
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

void TrayPopupUtils::ConfigureContainer(TriView::Container container,
                                        views::View* container_view) {
  container_view->SetBorder(CreateDefaultBorder(container));
  container_view->SetLayoutManager(
      CreateDefaultLayoutManager(container).release());
}

void TrayPopupUtils::ConfigureDefaultSizeAndFlex(TriView* tri_view,
                                                 TriView::Container container) {
  switch (container) {
    case TriView::Container::START:
      tri_view->SetMinSize(
          TriView::Container::START,
          gfx::Size(GetTrayConstant(TRAY_POPUP_ITEM_MIN_START_WIDTH), 0));
      break;
    case TriView::Container::CENTER:
      tri_view->SetFlexForContainer(TriView::Container::CENTER, 1.f);
      break;
    case TriView::Container::END:
      tri_view->SetMinSize(
          TriView::Container::END,
          gfx::Size(GetTrayConstant(TRAY_POPUP_ITEM_MIN_END_WIDTH), 0));
      break;
  }
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

bool TrayPopupUtils::CanOpenWebUISettings(LoginStatus status) {
  // TODO(tdanderson): Consider moving this into WmShell, or introduce a
  // CanShowSettings() method in each delegate type that has a
  // ShowSettings() method.
  return status != LoginStatus::NOT_LOGGED_IN &&
         status != LoginStatus::LOCKED &&
         !WmShell::Get()->GetSessionStateDelegate()->IsInSecondaryLoginScreen();
}

std::unique_ptr<views::LayoutManager>
TrayPopupUtils::CreateDefaultLayoutManager(TriView::Container container) {
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

std::unique_ptr<views::Border> TrayPopupUtils::CreateDefaultBorder(
    TriView::Container container) {
  switch (container) {
    case TriView::Container::START:
      // TODO(bruthig): Update the 'Main' images to have a fixed size that is
      // just the painted size and add a border.
      return nullptr;
      break;
    case TriView::Container::CENTER:
      return nullptr;
      break;
    case TriView::Container::END:
      return views::CreateEmptyBorder(
          0, GetTrayConstant(TRAY_POPUP_ITEM_MORE_REGION_HORIZONTAL_INSET), 0,
          GetTrayConstant(TRAY_POPUP_ITEM_MORE_REGION_HORIZONTAL_INSET));
      break;
  }
  // Required by some compilers.
  NOTREACHED();
  return nullptr;
}

}  // namespace ash
