// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray/tray_views.h"

#include "ash/ash_constants.h"
#include "ash/system/tray/fixed_sized_image_view.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_item_view.h"
#include "base/i18n/rtl.h"
#include "grit/ash_resources.h"
#include "grit/ash_strings.h"
#include "grit/ui_resources.h"
#include "ui/base/accessibility/accessible_view_state.h"
#include "ui/base/events/event.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/text_constants.h"
#include "ui/gfx/vector2d.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/painter.h"

namespace ash {
namespace internal {

namespace {
const int kTrayPopupLabelButtonPaddingHorizontal = 16;
const int kTrayPopupLabelButtonPaddingVertical = 8;

// Time in ms per throbber frame.
const int kThrobberFrameMs = 30;

// Duration for showing/hiding animation in milliseconds.
const int kThrobberAnimationDurationMs = 200;

const int kBarImagesActive[] = {
    IDR_SLIDER_ACTIVE_LEFT,
    IDR_SLIDER_ACTIVE_CENTER,
    IDR_SLIDER_ACTIVE_RIGHT,
};

const int kBarImagesDisabled[] = {
    IDR_SLIDER_DISABLED_LEFT,
    IDR_SLIDER_DISABLED_CENTER,
    IDR_SLIDER_DISABLED_RIGHT,
};

const int kTrayPopupLabelButtonBorderImagesNormal[] = {
    IDR_AURA_TRAY_POPUP_LABEL_BUTTON_BORDER,
    IDR_AURA_TRAY_POPUP_LABEL_BUTTON_NORMAL_BACKGROUND,
    IDR_AURA_TRAY_POPUP_LABEL_BUTTON_NORMAL_BACKGROUND,
    IDR_AURA_TRAY_POPUP_LABEL_BUTTON_BORDER,
    IDR_AURA_TRAY_POPUP_LABEL_BUTTON_NORMAL_BACKGROUND,
    IDR_AURA_TRAY_POPUP_LABEL_BUTTON_NORMAL_BACKGROUND,
    IDR_AURA_TRAY_POPUP_LABEL_BUTTON_BORDER,
    IDR_AURA_TRAY_POPUP_LABEL_BUTTON_NORMAL_BACKGROUND,
    IDR_AURA_TRAY_POPUP_LABEL_BUTTON_NORMAL_BACKGROUND,
};
const int kTrayPopupLabelButtonBorderImagesHovered[] = {
    IDR_AURA_TRAY_POPUP_LABEL_BUTTON_BORDER,
    IDR_AURA_TRAY_POPUP_LABEL_BUTTON_BORDER,
    IDR_AURA_TRAY_POPUP_LABEL_BUTTON_BORDER,
    IDR_AURA_TRAY_POPUP_LABEL_BUTTON_BORDER,
    IDR_AURA_TRAY_POPUP_LABEL_BUTTON_HOVER_BACKGROUND,
    IDR_AURA_TRAY_POPUP_LABEL_BUTTON_BORDER,
    IDR_AURA_TRAY_POPUP_LABEL_BUTTON_BORDER,
    IDR_AURA_TRAY_POPUP_LABEL_BUTTON_BORDER,
    IDR_AURA_TRAY_POPUP_LABEL_BUTTON_BORDER,
};

}

////////////////////////////////////////////////////////////////////////////////
// FixedSizedScrollView

FixedSizedScrollView::FixedSizedScrollView() {
  set_focusable(true);
  set_notify_enter_exit_on_child(true);
}

FixedSizedScrollView::~FixedSizedScrollView() {}

void FixedSizedScrollView::SetContentsView(View* view) {
  SetContents(view);
  view->SetBoundsRect(gfx::Rect(view->GetPreferredSize()));
}

void FixedSizedScrollView::SetFixedSize(const gfx::Size& size) {
  if (fixed_size_ == size)
    return;
  fixed_size_ = size;
  PreferredSizeChanged();
}

gfx::Size FixedSizedScrollView::GetPreferredSize() {
  gfx::Size size = fixed_size_.IsEmpty() ?
      contents()->GetPreferredSize() : fixed_size_;
  gfx::Insets insets = GetInsets();
  size.Enlarge(insets.width(), insets.height());
  return size;
}

void FixedSizedScrollView::Layout() {
  gfx::Rect bounds = gfx::Rect(contents()->GetPreferredSize());
  bounds.set_width(std::max(0, width() - GetScrollBarWidth()));
  contents()->SetBoundsRect(bounds);

  views::ScrollView::Layout();
  if (!vertical_scroll_bar()->visible()) {
    gfx::Rect bounds = contents()->bounds();
    bounds.set_width(bounds.width() + GetScrollBarWidth());
    contents()->SetBoundsRect(bounds);
  }
}

void FixedSizedScrollView::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  gfx::Rect bounds = gfx::Rect(contents()->GetPreferredSize());
  bounds.set_width(std::max(0, width() - GetScrollBarWidth()));
  contents()->SetBoundsRect(bounds);
}

void FixedSizedScrollView::OnMouseEntered(const ui::MouseEvent& event) {
  // TODO(sad): This is done to make sure that the scroll view scrolls on
  // mouse-wheel events. This is ugly, and Ben thinks this is weird. There
  // should be a better fix for this.
  RequestFocus();
}

void FixedSizedScrollView::OnPaintFocusBorder(gfx::Canvas* canvas) {
  // Do not paint the focus border.
}

////////////////////////////////////////////////////////////////////////////////
// TrayPopupLabelButtonBorder

TrayPopupLabelButtonBorder::TrayPopupLabelButtonBorder()
    : LabelButtonBorder(views::Button::STYLE_TEXTBUTTON) {
  SetPainter(false, views::Button::STATE_NORMAL,
             views::Painter::CreateImageGridPainter(
                 kTrayPopupLabelButtonBorderImagesNormal));
  SetPainter(false, views::Button::STATE_DISABLED,
             views::Painter::CreateImageGridPainter(
                 kTrayPopupLabelButtonBorderImagesNormal));
  SetPainter(false, views::Button::STATE_HOVERED,
             views::Painter::CreateImageGridPainter(
                 kTrayPopupLabelButtonBorderImagesHovered));
  SetPainter(false, views::Button::STATE_PRESSED,
             views::Painter::CreateImageGridPainter(
                 kTrayPopupLabelButtonBorderImagesHovered));
}

TrayPopupLabelButtonBorder::~TrayPopupLabelButtonBorder() {}

void TrayPopupLabelButtonBorder::Paint(const views::View& view,
                                       gfx::Canvas* canvas) {
  const views::NativeThemeDelegate* native_theme_delegate =
      static_cast<const views::LabelButton*>(&view);
  ui::NativeTheme::ExtraParams extra;
  const ui::NativeTheme::State state =
      native_theme_delegate->GetThemeState(&extra);
  if (state == ui::NativeTheme::kNormal ||
      state == ui::NativeTheme::kDisabled) {
    // In normal and disabled state, the border is a vertical bar separating the
    // button from the preceding sibling. If this button is its parent's first
    // visible child, the separator bar should be omitted.
    const views::View* first_visible_child = NULL;
    for (int i = 0; i < view.parent()->child_count(); ++i) {
      const views::View* child = view.parent()->child_at(i);
      if (child->visible()) {
        first_visible_child = child;
        break;
      }
    }
    if (first_visible_child == &view)
      return;
  }
  if (base::i18n::IsRTL()) {
    canvas->Save();
    canvas->Translate(gfx::Vector2d(view.width(), 0));
    canvas->Scale(-1, 1);
    LabelButtonBorder::Paint(view, canvas);
    canvas->Restore();
  } else {
    LabelButtonBorder::Paint(view, canvas);
  }
}

gfx::Insets TrayPopupLabelButtonBorder::GetInsets() const {
  return gfx::Insets(kTrayPopupLabelButtonPaddingVertical,
                     kTrayPopupLabelButtonPaddingHorizontal,
                     kTrayPopupLabelButtonPaddingVertical,
                     kTrayPopupLabelButtonPaddingHorizontal);
}

////////////////////////////////////////////////////////////////////////////////
// TrayPopupLabelButton

TrayPopupLabelButton::TrayPopupLabelButton(views::ButtonListener* listener,
                                           const string16& text)
    : views::LabelButton(listener, text) {
  set_border(new TrayPopupLabelButtonBorder);
  set_focusable(true);
  set_request_focus_on_press(false);
  set_animate_on_state_change(false);
  SetHorizontalAlignment(gfx::ALIGN_CENTER);
}

TrayPopupLabelButton::~TrayPopupLabelButton() {}

void TrayPopupLabelButton::OnPaintFocusBorder(gfx::Canvas* canvas) {
  if (HasFocus() && (focusable() || IsAccessibilityFocusable())) {
    canvas->DrawRect(gfx::Rect(1, 1, width() - 3, height() - 3),
                     ash::kFocusBorderColor);
  }
}

////////////////////////////////////////////////////////////////////////////////
// TrayPopupHeaderButton

TrayPopupHeaderButton::TrayPopupHeaderButton(views::ButtonListener* listener,
                                             int enabled_resource_id,
                                             int disabled_resource_id,
                                             int enabled_resource_id_hover,
                                             int disabled_resource_id_hover,
                                             int accessible_name_id)
    : views::ToggleImageButton(listener) {
  ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
  SetImage(views::Button::STATE_NORMAL,
      bundle.GetImageNamed(enabled_resource_id).ToImageSkia());
  SetToggledImage(views::Button::STATE_NORMAL,
      bundle.GetImageNamed(disabled_resource_id).ToImageSkia());
  SetImage(views::Button::STATE_HOVERED,
      bundle.GetImageNamed(enabled_resource_id_hover).ToImageSkia());
  SetToggledImage(views::Button::STATE_HOVERED,
      bundle.GetImageNamed(disabled_resource_id_hover).ToImageSkia());
  SetImageAlignment(views::ImageButton::ALIGN_CENTER,
                    views::ImageButton::ALIGN_MIDDLE);
  SetAccessibleName(bundle.GetLocalizedString(accessible_name_id));
  set_focusable(true);
  set_request_focus_on_press(false);
}

TrayPopupHeaderButton::~TrayPopupHeaderButton() {}

gfx::Size TrayPopupHeaderButton::GetPreferredSize() {
  return gfx::Size(ash::kTrayPopupItemHeight, ash::kTrayPopupItemHeight);
}

void TrayPopupHeaderButton::OnPaintBorder(gfx::Canvas* canvas) {
  // Just the left border.
  const int kBorderHeight = 25;
  int padding = (height() - kBorderHeight) / 2;
  canvas->FillRect(gfx::Rect(0, padding, 1, height() - padding * 2),
      ash::kBorderDarkColor);
}

void TrayPopupHeaderButton::OnPaintFocusBorder(gfx::Canvas* canvas) {
  if (HasFocus() && (focusable() || IsAccessibilityFocusable())) {
    canvas->DrawRect(gfx::Rect(2, 1, width() - 4, height() - 3),
                     kFocusBorderColor);
  }
}

void TrayPopupHeaderButton::StateChanged() {
  SchedulePaint();
}

////////////////////////////////////////////////////////////////////////////////
// TrayBarButtonWithTitle

class TrayBarButtonWithTitle::TrayBarButton
  : public views::View {
 public:
  TrayBarButton(const int bar_active_images[], const int bar_disabled_images[])
    : views::View(),
      bar_active_images_(bar_active_images),
      bar_disabled_images_(bar_disabled_images),
      painter_(new views::HorizontalPainter(bar_active_images_)){
  }
  virtual ~TrayBarButton() {}

  // Overriden from views::View
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE {
    painter_->Paint(canvas, size());
  }

  void Update(bool control_on) {
    painter_.reset(new views::HorizontalPainter(
        control_on ? bar_active_images_ : bar_disabled_images_));
    SchedulePaint();
  }

 private:
  const int* bar_active_images_;
  const int* bar_disabled_images_;
  scoped_ptr<views::HorizontalPainter> painter_;

  DISALLOW_COPY_AND_ASSIGN(TrayBarButton);
};

TrayBarButtonWithTitle::TrayBarButtonWithTitle(views::ButtonListener* listener,
                                               int title_id,
                                               int width)
    : views::CustomButton(listener),
      image_(new TrayBarButton(kBarImagesActive, kBarImagesDisabled)),
      title_(NULL),
      width_(width) {
  AddChildView(image_);
  if (title_id != -1) {
    title_ = new views::Label;
    ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
    string16 text = rb.GetLocalizedString(title_id);
    title_->SetText(text);
    AddChildView(title_);
  }

  image_height_ = ui::ResourceBundle::GetSharedInstance().GetImageNamed(
      kBarImagesActive[0]).ToImageSkia()->height();
}

TrayBarButtonWithTitle::~TrayBarButtonWithTitle() {}

gfx::Size TrayBarButtonWithTitle::GetPreferredSize() {
  return gfx::Size(width_, kTrayPopupItemHeight);
}

void TrayBarButtonWithTitle::Layout() {
  gfx::Rect rect(GetContentsBounds());
  int bar_image_y = rect.height() / 2 - image_height_ / 2;
  gfx::Rect bar_image_rect(rect.x(),
                           bar_image_y,
                           rect.width(),
                           image_height_);
  image_->SetBoundsRect(bar_image_rect);
  if (title_) {
    // The image_ has some empty space below the bar image, move the title
    // a little bit up to look closer to the bar.
    gfx::Size title_size = title_->GetPreferredSize();
    title_->SetBounds(rect.x(),
                      bar_image_y + image_height_ - 3,
                      rect.width(),
                      title_size.height());
  }
}

void TrayBarButtonWithTitle::UpdateButton(bool control_on) {
  image_->Update(control_on);
}

SystemTrayThrobber::SystemTrayThrobber(int frame_delay_ms)
    : views::SmoothedThrobber(frame_delay_ms) {
}

SystemTrayThrobber::~SystemTrayThrobber() {
}

void SystemTrayThrobber::SetTooltipText(const string16& tooltip_text) {
  tooltip_text_ = tooltip_text;
}

bool SystemTrayThrobber::GetTooltipText(const gfx::Point& p,
                                        string16* tooltip) const {
  if (tooltip_text_.empty())
    return false;

  *tooltip = tooltip_text_;
  return true;
}

ThrobberView::ThrobberView() {
  throbber_ = new SystemTrayThrobber(kThrobberFrameMs);
  throbber_->SetFrames(ui::ResourceBundle::GetSharedInstance().GetImageNamed(
      IDR_AURA_CROS_DEFAULT_THROBBER).ToImageSkia());
  throbber_->set_stop_delay_ms(kThrobberAnimationDurationMs);
  AddChildView(throbber_);

  SetPaintToLayer(true);
  layer()->SetFillsBoundsOpaquely(false);
  layer()->SetOpacity(0.0);
}

ThrobberView::~ThrobberView() {
}

gfx::Size ThrobberView::GetPreferredSize() {
  return gfx::Size(ash::kTrayPopupItemHeight, ash::kTrayPopupItemHeight);
}

void ThrobberView::Layout() {
  View* child = child_at(0);
  gfx::Size ps = child->GetPreferredSize();
  child->SetBounds((width() - ps.width()) / 2,
                   (height() - ps.height()) / 2,
                   ps.width(), ps.height());
  SizeToPreferredSize();
}

bool ThrobberView::GetTooltipText(const gfx::Point& p,
                                  string16* tooltip) const {
  if (tooltip_text_.empty())
    return false;

  *tooltip = tooltip_text_;
  return true;
}

void ThrobberView::Start() {
  ScheduleAnimation(true);
  throbber_->Start();
}

void ThrobberView::Stop() {
  ScheduleAnimation(false);
  throbber_->Stop();
}

void ThrobberView::SetTooltipText(const string16& tooltip_text) {
  tooltip_text_ = tooltip_text;
  throbber_->SetTooltipText(tooltip_text);
}

void ThrobberView::ScheduleAnimation(bool start_throbber) {
  // Stop any previous animation.
  layer()->GetAnimator()->StopAnimating();

  ui::ScopedLayerAnimationSettings animation(layer()->GetAnimator());
  animation.SetTransitionDuration(
      base::TimeDelta::FromMilliseconds(kThrobberAnimationDurationMs));

  layer()->SetOpacity(start_throbber ? 1.0 : 0.0);
}

void SetupLabelForTray(views::Label* label) {
  // Making label_font static to avoid the time penalty of DeriveFont for
  // all but the first call.
  static const gfx::Font label_font(gfx::Font().DeriveFont(1, gfx::Font::BOLD));
  label->SetFont(label_font);
  label->SetAutoColorReadabilityEnabled(false);
  label->SetEnabledColor(SK_ColorWHITE);
  label->SetBackgroundColor(SkColorSetARGB(0, 255, 255, 255));
  label->SetShadowColors(SkColorSetARGB(64, 0, 0, 0),
                         SkColorSetARGB(64, 0, 0, 0));
  label->SetShadowOffset(0, 1);
}

void SetTrayImageItemBorder(views::View* tray_view,
                            ShelfAlignment alignment) {
  if (alignment == SHELF_ALIGNMENT_BOTTOM ||
      alignment == SHELF_ALIGNMENT_TOP) {
    tray_view->set_border(views::Border::CreateEmptyBorder(
        0, kTrayImageItemHorizontalPaddingBottomAlignment,
        0, kTrayImageItemHorizontalPaddingBottomAlignment));
  } else {
    tray_view->set_border(views::Border::CreateEmptyBorder(
        kTrayImageItemVerticalPaddingVerticalAlignment,
        kTrayImageItemHorizontalPaddingVerticalAlignment,
        kTrayImageItemVerticalPaddingVerticalAlignment,
        kTrayImageItemHorizontalPaddingVerticalAlignment));
  }
}

void SetTrayLabelItemBorder(TrayItemView* tray_view,
                            ShelfAlignment alignment) {
  if (alignment == SHELF_ALIGNMENT_BOTTOM ||
      alignment == SHELF_ALIGNMENT_TOP) {
    tray_view->set_border(views::Border::CreateEmptyBorder(
        0, kTrayLabelItemHorizontalPaddingBottomAlignment,
        0, kTrayLabelItemHorizontalPaddingBottomAlignment));
  } else {
    // Center the label for vertical launcher alignment.
    int horizontal_padding = (tray_view->GetPreferredSize().width() -
        tray_view->label()->GetPreferredSize().width()) / 2;
    tray_view->set_border(views::Border::CreateEmptyBorder(
        kTrayLabelItemVerticalPaddingVeriticalAlignment,
        horizontal_padding,
        kTrayLabelItemVerticalPaddingVeriticalAlignment,
        horizontal_padding));
  }
}

}  // namespace internal
}  // namespace ash
