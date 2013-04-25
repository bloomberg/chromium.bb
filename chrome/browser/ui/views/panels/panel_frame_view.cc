// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/panels/panel_frame_view.h"

#include "chrome/browser/ui/panels/panel.h"
#include "chrome/browser/ui/panels/panel_constants.h"
#include "chrome/browser/ui/views/panels/panel_view.h"
#include "chrome/browser/ui/views/tab_icon_view.h"
#include "content/public/browser/web_contents.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "grit/ui_resources.h"
#include "ui/base/hit_test.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/path.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

#if defined(OS_WIN)
#include "base/win/scoped_gdi_object.h"
#include "ui/base/win/shell.h"
#include "ui/gfx/path_win.h"
#include "ui/views/win/hwnd_util.h"
#endif

#if defined(USE_AURA)
#include "ui/aura/window.h"
#endif

namespace {

// The thickness of the border when Aero is not enabled. In this case, the
// shadow around the window will not be painted by the system and we need to
// paint a frame in order to differentiate the client area from the background.
const int kNonAeroBorderThickness = 1;

// The height and width in pixels of the icon.
const int kIconSize = 16;

// The font to use to draw the title.
const char* kTitleFontName = "Arial Bold";
const int kTitleFontSize = 14;

// The extra padding between the button and the top edge.
const int kExtraPaddingBetweenButtonAndTop = 1;

// Colors used to draw titlebar background under default theme.
const SkColor kActiveBackgroundDefaultColor = SkColorSetRGB(0x3a, 0x3d, 0x3d);
const SkColor kInactiveBackgroundDefaultColor = SkColorSetRGB(0x7a, 0x7c, 0x7c);
const SkColor kAttentionBackgroundDefaultColor =
    SkColorSetRGB(0x53, 0xa9, 0x3f);

// Color used to draw the minimized panel.
const SkColor kMinimizeBackgroundDefaultColor = SkColorSetRGB(0xf5, 0xf4, 0xf0);
const SkColor kMinimizeBorderDefaultColor = SkColorSetRGB(0xc9, 0xc9, 0xc9);

// Color used to draw the title text under default theme.
const SkColor kTitleTextDefaultColor = SkColorSetRGB(0xf9, 0xf9, 0xf9);

gfx::ImageSkia* CreateImageForColor(SkColor color) {
  gfx::Canvas canvas(gfx::Size(1, 1), ui::SCALE_FACTOR_100P, true);
  canvas.DrawColor(color);
  return new gfx::ImageSkia(canvas.ExtractImageRep());
}

const gfx::ImageSkia& GetTopLeftCornerImage(panel::CornerStyle corner_style) {
  static gfx::ImageSkia* rounded_image = NULL;
  static gfx::ImageSkia* non_rounded_image = NULL;
  if (!rounded_image) {
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    rounded_image = rb.GetImageSkiaNamed(IDR_WINDOW_TOP_LEFT_CORNER);
    non_rounded_image = rb.GetImageSkiaNamed(IDR_PANEL_TOP_LEFT_CORNER);
  }
  return (corner_style & panel::TOP_ROUNDED) ? *rounded_image
                                             : *non_rounded_image;
}

const gfx::ImageSkia& GetTopRightCornerImage(panel::CornerStyle corner_style) {
  static gfx::ImageSkia* rounded_image = NULL;
  static gfx::ImageSkia* non_rounded_image = NULL;
  if (!rounded_image) {
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    rounded_image = rb.GetImageSkiaNamed(IDR_WINDOW_TOP_RIGHT_CORNER);
    non_rounded_image = rb.GetImageSkiaNamed(IDR_PANEL_TOP_RIGHT_CORNER);
  }
  return (corner_style & panel::TOP_ROUNDED) ? *rounded_image
                                             : *non_rounded_image;
}

const gfx::ImageSkia& GetBottomLeftCornerImage(
    panel::CornerStyle corner_style) {
  static gfx::ImageSkia* rounded_image = NULL;
  static gfx::ImageSkia* non_rounded_image = NULL;
  if (!rounded_image) {
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    rounded_image = rb.GetImageSkiaNamed(IDR_WINDOW_BOTTOM_LEFT_CORNER);
    non_rounded_image = rb.GetImageSkiaNamed(IDR_PANEL_BOTTOM_LEFT_CORNER);
  }
  return (corner_style & panel::BOTTOM_ROUNDED) ? *rounded_image
                                                : *non_rounded_image;
}

const gfx::ImageSkia& GetBottomRightCornerImage(
    panel::CornerStyle corner_style) {
  static gfx::ImageSkia* rounded_image = NULL;
  static gfx::ImageSkia* non_rounded_image = NULL;
  if (!rounded_image) {
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    rounded_image = rb.GetImageSkiaNamed(IDR_WINDOW_BOTTOM_RIGHT_CORNER);
    non_rounded_image = rb.GetImageSkiaNamed(IDR_PANEL_BOTTOM_RIGHT_CORNER);
  }
  return (corner_style & panel::BOTTOM_ROUNDED) ? *rounded_image
                                                : *non_rounded_image;
}

const gfx::ImageSkia& GetTopEdgeImage() {
  static gfx::ImageSkia* image = NULL;
  if (!image) {
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    image = rb.GetImageSkiaNamed(IDR_WINDOW_TOP_CENTER);
  }
  return *image;
}

const gfx::ImageSkia& GetBottomEdgeImage() {
  static gfx::ImageSkia* image = NULL;
  if (!image) {
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    image = rb.GetImageSkiaNamed(IDR_WINDOW_BOTTOM_CENTER);
  }
  return *image;
}

const gfx::ImageSkia& GetLeftEdgeImage() {
  static gfx::ImageSkia* image = NULL;
  if (!image) {
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    image = rb.GetImageSkiaNamed(IDR_WINDOW_LEFT_SIDE);
  }
  return *image;
}

const gfx::ImageSkia& GetRightEdgeImage() {
  static gfx::ImageSkia* image = NULL;
  if (!image) {
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    image = rb.GetImageSkiaNamed(IDR_WINDOW_RIGHT_SIDE);
  }
  return *image;
}

const gfx::Font& GetTitleFont() {
  static gfx::Font* font = NULL;
  if (!font)
    font = new gfx::Font(kTitleFontName, kTitleFontSize);
  return *font;
}

const gfx::ImageSkia* GetActiveBackgroundDefaultImage() {
  static gfx::ImageSkia* image = NULL;
  if (!image)
    image = CreateImageForColor(kActiveBackgroundDefaultColor);
  return image;
}

const gfx::ImageSkia* GetInactiveBackgroundDefaultImage() {
  static gfx::ImageSkia* image = NULL;
  if (!image)
    image = CreateImageForColor(kInactiveBackgroundDefaultColor);
  return image;
}

const gfx::ImageSkia* GetAttentionBackgroundDefaultImage() {
  static gfx::ImageSkia* image = NULL;
  if (!image)
    image = CreateImageForColor(kAttentionBackgroundDefaultColor);
  return image;
}

const gfx::ImageSkia* GetMinimizeBackgroundDefaultImage() {
  static gfx::ImageSkia* image = NULL;
  if (!image)
    image = CreateImageForColor(kMinimizeBackgroundDefaultColor);
  return image;
}

int GetFrameEdgeHitTest(const gfx::Point& point,
                        const gfx::Size& frame_size,
                        int resize_area_size,
                        panel::Resizability resizability) {
  int x = point.x();
  int y = point.y();
  int width = frame_size.width();
  int height = frame_size.height();
  if (x < resize_area_size) {
    if (y < resize_area_size && (resizability & panel::RESIZABLE_TOP_LEFT)) {
      return HTTOPLEFT;
    } else if (y >= height - resize_area_size &&
              (resizability & panel::RESIZABLE_BOTTOM_LEFT)) {
      return HTBOTTOMLEFT;
    } else if (resizability & panel::RESIZABLE_LEFT) {
      return HTLEFT;
    }
  } else if (x >= width - resize_area_size) {
    if (y < resize_area_size && (resizability & panel::RESIZABLE_TOP_RIGHT)) {
      return HTTOPRIGHT;
    } else if (y >= height - resize_area_size &&
              (resizability & panel::RESIZABLE_BOTTOM_RIGHT)) {
      return HTBOTTOMRIGHT;
    } else if (resizability & panel::RESIZABLE_RIGHT) {
      return HTRIGHT;
    }
  }

  if (y < resize_area_size && (resizability & panel::RESIZABLE_TOP)) {
    return HTTOP;
  } else if (y >= height - resize_area_size &&
            (resizability & panel::RESIZABLE_BOTTOM)) {
    return HTBOTTOM;
  }

  return HTNOWHERE;
}

// Frameless is only supported when Aero is enabled and shadow effect is
// present.
bool ShouldRenderAsFrameless() {
#if defined(OS_WIN)
  bool is_frameless = ui::win::IsAeroGlassEnabled();
  if (is_frameless) {
    BOOL shadow_enabled = FALSE;
    if (::SystemParametersInfo(SPI_GETDROPSHADOW, 0, &shadow_enabled, 0) &&
        !shadow_enabled)
      is_frameless = false;
  }
  return is_frameless;
#else
  return false;
#endif
}

}  // namespace

const char PanelFrameView::kViewClassName[] =
    "browser/ui/panels/PanelFrameView";

PanelFrameView::PanelFrameView(PanelView* panel_view)
    : is_frameless_(ShouldRenderAsFrameless()),
      panel_view_(panel_view),
      close_button_(NULL),
      minimize_button_(NULL),
      restore_button_(NULL),
      title_icon_(NULL),
      title_label_(NULL),
      corner_style_(panel::ALL_ROUNDED) {
}

PanelFrameView::~PanelFrameView() {
}

void PanelFrameView::Init() {
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();

  close_button_ = new views::ImageButton(this);
  close_button_->SetImage(views::CustomButton::STATE_NORMAL,
                          rb.GetImageSkiaNamed(IDR_PANEL_CLOSE));
  close_button_->SetImage(views::CustomButton::STATE_HOVERED,
                          rb.GetImageSkiaNamed(IDR_PANEL_CLOSE_H));
  close_button_->SetImage(views::CustomButton::STATE_PRESSED,
                          rb.GetImageSkiaNamed(IDR_PANEL_CLOSE_C));
  close_button_->SetImageAlignment(views::ImageButton::ALIGN_CENTER,
                                   views::ImageButton::ALIGN_MIDDLE);
  string16 tooltip_text = l10n_util::GetStringUTF16(IDS_PANEL_CLOSE_TOOLTIP);
  close_button_->SetTooltipText(tooltip_text);
  AddChildView(close_button_);

  minimize_button_ = new views::ImageButton(this);
  minimize_button_->SetImage(views::CustomButton::STATE_NORMAL,
                             rb.GetImageSkiaNamed(IDR_PANEL_MINIMIZE));
  minimize_button_->SetImage(views::CustomButton::STATE_HOVERED,
                             rb.GetImageSkiaNamed(IDR_PANEL_MINIMIZE_H));
  minimize_button_->SetImage(views::CustomButton::STATE_PRESSED,
                             rb.GetImageSkiaNamed(IDR_PANEL_MINIMIZE_C));
  tooltip_text = l10n_util::GetStringUTF16(IDS_PANEL_MINIMIZE_TOOLTIP);
  minimize_button_->SetTooltipText(tooltip_text);
  minimize_button_->SetImageAlignment(views::ImageButton::ALIGN_CENTER,
                                      views::ImageButton::ALIGN_MIDDLE);
  AddChildView(minimize_button_);

  restore_button_ = new views::ImageButton(this);
  restore_button_->SetImage(views::CustomButton::STATE_NORMAL,
                            rb.GetImageSkiaNamed(IDR_PANEL_RESTORE));
  restore_button_->SetImage(views::CustomButton::STATE_HOVERED,
                            rb.GetImageSkiaNamed(IDR_PANEL_RESTORE_H));
  restore_button_->SetImage(views::CustomButton::STATE_PRESSED,
                            rb.GetImageSkiaNamed(IDR_PANEL_RESTORE_C));
  restore_button_->SetImageAlignment(views::ImageButton::ALIGN_CENTER,
                                     views::ImageButton::ALIGN_MIDDLE);
  tooltip_text = l10n_util::GetStringUTF16(IDS_PANEL_RESTORE_TOOLTIP);
  restore_button_->SetTooltipText(tooltip_text);
  restore_button_->SetVisible(false);  // only visible when panel is minimized
  AddChildView(restore_button_);

  title_icon_ = new TabIconView(this);
  title_icon_->set_is_light(true);
  AddChildView(title_icon_);
  title_icon_->Update();

  title_label_ = new views::Label(panel_view_->panel()->GetWindowTitle());
  title_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  title_label_->SetAutoColorReadabilityEnabled(false);
  title_label_->SetFont(GetTitleFont());
  AddChildView(title_label_);

#if defined(USE_AURA)
  // Compute the thickness of the client area that needs to be counted towards
  // mouse resizing.
  int thickness_for_mouse_resizing =
      PanelView::kResizeInsideBoundsSize - BorderThickness();
  aura::Window* window = panel_view_->GetNativePanelWindow();
  window->set_hit_test_bounds_override_inner(
      gfx::Insets(thickness_for_mouse_resizing, thickness_for_mouse_resizing,
                  thickness_for_mouse_resizing, thickness_for_mouse_resizing));
#endif
}

void PanelFrameView::UpdateTitle() {
  UpdateWindowTitle();
}

void PanelFrameView::UpdateIcon() {
  UpdateWindowIcon();
}

void PanelFrameView::UpdateThrobber() {
  title_icon_->Update();
}

void PanelFrameView::UpdateTitlebarMinimizeRestoreButtonVisibility() {
  Panel* panel = panel_view_->panel();
  minimize_button_->SetVisible(panel->CanShowMinimizeButton());
  restore_button_->SetVisible(panel->CanShowRestoreButton());

  // Reset the button states in case that the hover states are not cleared when
  // mouse is clicked but not moved.
  minimize_button_->SetState(views::CustomButton::STATE_NORMAL);
  restore_button_->SetState(views::CustomButton::STATE_NORMAL);
}

void PanelFrameView::SetWindowCornerStyle(panel::CornerStyle corner_style) {
  corner_style_ = corner_style;

#if defined(OS_WIN)
  // Changing the window region is going to force a paint. Only change the
  // window region if the region really differs.
  HWND native_window = views::HWNDForWidget(panel_view_->window());
  base::win::ScopedRegion current_region(::CreateRectRgn(0, 0, 0, 0));
  int current_region_result = ::GetWindowRgn(native_window, current_region);

  gfx::Path window_mask;
  GetWindowMask(size(), &window_mask);
  base::win::ScopedRegion new_region(gfx::CreateHRGNFromSkPath(window_mask));

  if (current_region_result == ERROR ||
      !::EqualRgn(current_region, new_region)) {
    // SetWindowRgn takes ownership of the new_region.
    ::SetWindowRgn(native_window, new_region.release(), TRUE);
  }
#endif
}

gfx::Rect PanelFrameView::GetBoundsForClientView() const {
  // The origin of client-area bounds starts after left border and titlebar and
  // spans until hitting the right and bottom borders.
  //    +------------------------------+
  //    |         Top Titlebar         |
  //    |-+--------------------------+-|
  //    |L|                          |R|
  //    |e|                          |i|
  //    |f|                          |g|
  //    |t|                          |h|
  //    | |         Client           |t|
  //    | |                          | |
  //    |B|          Area            |B|
  //    |o|                          |o|
  //    |r|                          |r|
  //    |d|                          |d|
  //    |e|                          |e|
  //    |r|                          |r|
  //    | +--------------------------+ |
  //    |        Bottom Border         |
  //    +------------------------------+
  int titlebar_height = TitlebarHeight();
  int border_thickness = BorderThickness();
  return gfx::Rect(border_thickness,
                   titlebar_height,
                   std::max(0, width() - border_thickness * 2),
                   std::max(0, height() - titlebar_height - border_thickness));
}

gfx::Rect PanelFrameView::GetWindowBoundsForClientBounds(
      const gfx::Rect& client_bounds) const {
  int titlebar_height = TitlebarHeight();
  int border_thickness = BorderThickness();
  // The window bounds include both client area and non-client area (titlebar
  // and left, right and bottom borders).
  return gfx::Rect(client_bounds.x() - border_thickness,
                   client_bounds.y() - titlebar_height,
                   client_bounds.width() + border_thickness * 2,
                   client_bounds.height() + titlebar_height + border_thickness);
}

int PanelFrameView::NonClientHitTest(const gfx::Point& point) {
  panel::Resizability resizability = panel_view_->panel()->CanResizeByMouse();

  // Check the frame first, as we allow a small area overlapping the contents
  // to be used for resize handles.
  int frame_component = GetFrameEdgeHitTest(
      point, size(), PanelView::kResizeInsideBoundsSize, resizability);

  if (frame_component != HTNOWHERE)
    return frame_component;

  int client_component =
      panel_view_->window()->client_view()->NonClientHitTest(point);
  if (client_component != HTNOWHERE)
    return client_component;

  if (close_button_ && close_button_->visible() &&
      close_button_->GetMirroredBounds().Contains(point))
    return HTCLOSE;

  if (minimize_button_ && minimize_button_->visible() &&
      minimize_button_->GetMirroredBounds().Contains(point))
    return HTMINBUTTON;

  if (restore_button_ && restore_button_->visible() &&
      restore_button_->GetMirroredBounds().Contains(point))
    return HTMAXBUTTON;

  return HTNOWHERE;
}

void PanelFrameView::GetWindowMask(const gfx::Size& size,
                                   gfx::Path* window_mask) {
  int width = size.width();
  int height = size.height();

  if (corner_style_ & panel::TOP_ROUNDED) {
    window_mask->moveTo(0, 3);
    window_mask->lineTo(1, 2);
    window_mask->lineTo(1, 1);
    window_mask->lineTo(2, 1);
    window_mask->lineTo(3, 0);
    window_mask->lineTo(SkIntToScalar(width - 3), 0);
    window_mask->lineTo(SkIntToScalar(width - 2), 1);
    window_mask->lineTo(SkIntToScalar(width - 1), 1);
    window_mask->lineTo(SkIntToScalar(width - 1), 2);
    window_mask->lineTo(SkIntToScalar(width - 1), 3);
  } else {
    window_mask->moveTo(0, 0);
    window_mask->lineTo(width, 0);
  }

  if (corner_style_ & panel::BOTTOM_ROUNDED) {
    window_mask->lineTo(SkIntToScalar(width - 1), SkIntToScalar(height - 4));
    window_mask->lineTo(SkIntToScalar(width - 2), SkIntToScalar(height - 3));
    window_mask->lineTo(SkIntToScalar(width - 2), SkIntToScalar(height - 2));
    window_mask->lineTo(SkIntToScalar(width - 3), SkIntToScalar(height - 2));
    window_mask->lineTo(SkIntToScalar(width - 4), SkIntToScalar(height - 1));
    window_mask->lineTo(3, SkIntToScalar(height - 1));
    window_mask->lineTo(2, SkIntToScalar(height - 2));
    window_mask->lineTo(1, SkIntToScalar(height - 2));
    window_mask->lineTo(1, SkIntToScalar(height - 3));
    window_mask->lineTo(0, SkIntToScalar(height - 4));
  } else {
    window_mask->lineTo(SkIntToScalar(width), SkIntToScalar(height));
    window_mask->lineTo(0, SkIntToScalar(height));
  }

  window_mask->close();
}

void PanelFrameView::ResetWindowControls() {
  // The controls aren't affected by this constraint.
}

void PanelFrameView::UpdateWindowIcon() {
  title_icon_->SchedulePaint();
}

void PanelFrameView::UpdateWindowTitle() {
  title_label_->SetText(panel_view_->panel()->GetWindowTitle());
}

gfx::Size PanelFrameView::GetPreferredSize() {
  gfx::Size pref_size =
      panel_view_->window()->client_view()->GetPreferredSize();
  gfx::Rect bounds(0, 0, pref_size.width(), pref_size.height());
  return panel_view_->window()->non_client_view()->
      GetWindowBoundsForClientBounds(bounds).size();
}

std::string PanelFrameView::GetClassName() const {
  return kViewClassName;
}

gfx::Size PanelFrameView::GetMinimumSize() {
  return panel_view_->GetMinimumSize();
}

gfx::Size PanelFrameView::GetMaximumSize() {
  return panel_view_->GetMaximumSize();
}

void PanelFrameView::Layout() {
  is_frameless_ = ShouldRenderAsFrameless();

  // Layout the close button.
  int right = width();
  close_button_->SetBounds(
      width() - panel::kTitlebarRightPadding - panel::kPanelButtonSize,
      (TitlebarHeight() - panel::kPanelButtonSize) / 2 +
          kExtraPaddingBetweenButtonAndTop,
      panel::kPanelButtonSize,
      panel::kPanelButtonSize);
  right = close_button_->x();

  // Layout the minimize and restore button. Both occupy the same space,
  // but at most one is visible at any time.
  minimize_button_->SetBounds(
      right - panel::kButtonPadding - panel::kPanelButtonSize,
      (TitlebarHeight() - panel::kPanelButtonSize) / 2 +
          kExtraPaddingBetweenButtonAndTop,
      panel::kPanelButtonSize,
      panel::kPanelButtonSize);
  restore_button_->SetBoundsRect(minimize_button_->bounds());
  right = minimize_button_->x();

  // Layout the icon.
  int icon_y = (TitlebarHeight() - kIconSize) / 2;
  title_icon_->SetBounds(
      panel::kTitlebarLeftPadding,
      icon_y,
      kIconSize,
      kIconSize);

  // Layout the title.
  int title_x = title_icon_->bounds().right() + panel::kIconAndTitlePadding;
  int title_height = GetTitleFont().GetHeight();
  title_label_->SetBounds(
      title_x,
      icon_y + ((kIconSize - title_height - 1) / 2),
      std::max(0, right - panel::kTitleAndButtonPadding - title_x),
      title_height);
}

void PanelFrameView::OnPaint(gfx::Canvas* canvas) {
  UpdateControlStyles(GetPaintState());
  PaintFrameBackground(canvas);
  PaintFrameEdge(canvas);
}

bool PanelFrameView::OnMousePressed(const ui::MouseEvent& event) {
  if (event.IsOnlyLeftMouseButton()) {
    // |event.location| is in the view's coordinate system. Convert it to the
    // screen coordinate system.
    gfx::Point mouse_location = event.location();
    views::View::ConvertPointToScreen(this, &mouse_location);

    // If the mouse location falls within the resizing area of the titlebar,
    // do not handle the event so that the system resizing logic could kick in.
    if (!panel_view_->IsWithinResizingArea(mouse_location) &&
        panel_view_->OnTitlebarMousePressed(mouse_location))
      return true;
  }
  return NonClientFrameView::OnMousePressed(event);
}

bool PanelFrameView::OnMouseDragged(const ui::MouseEvent& event) {
  // |event.location| is in the view's coordinate system. Convert it to the
  // screen coordinate system.
  gfx::Point mouse_location = event.location();
  views::View::ConvertPointToScreen(this, &mouse_location);

  if (panel_view_->OnTitlebarMouseDragged(mouse_location))
    return true;
  return NonClientFrameView::OnMouseDragged(event);
}

void PanelFrameView::OnMouseReleased(const ui::MouseEvent& event) {
  if (panel_view_->OnTitlebarMouseReleased(
          event.IsControlDown() ? panel::APPLY_TO_ALL : panel::NO_MODIFIER))
    return;
  NonClientFrameView::OnMouseReleased(event);
}

void PanelFrameView::OnMouseCaptureLost() {
  if (panel_view_->OnTitlebarMouseCaptureLost())
    return;
  NonClientFrameView::OnMouseCaptureLost();
}

void PanelFrameView::ButtonPressed(views::Button* sender,
                                   const ui::Event& event) {
  if (sender == close_button_) {
    panel_view_->ClosePanel();
  } else {
    panel::ClickModifier modifier =
        event.IsControlDown() ? panel::APPLY_TO_ALL : panel::NO_MODIFIER;
    if (sender == minimize_button_)
      panel_view_->panel()->OnMinimizeButtonClicked(modifier);
    else if (sender == restore_button_)
      panel_view_->panel()->OnRestoreButtonClicked(modifier);
  }
}

bool PanelFrameView::ShouldTabIconViewAnimate() const {
  // This function is queried during the creation of the window as the
  // TabIconView we host is initialized, so we need to NULL check the selected
  // WebContents because in this condition there is not yet a selected tab.
  content::WebContents* contents = panel_view_->panel()->GetWebContents();
  return contents ? contents->IsLoading() : false;
}

gfx::ImageSkia PanelFrameView::GetFaviconForTabIconView() {
  return panel_view_->window()->widget_delegate()->GetWindowIcon();
}

gfx::Size PanelFrameView::NonClientAreaSize() const {
  if (is_frameless_)
    return gfx::Size(0, TitlebarHeight());
  // When the frame is present, the width of non-client area consists of
  // left and right borders, while the height consists of the top area
  // (titlebar) and the bottom border.
  return gfx::Size(2 * kNonAeroBorderThickness,
                   TitlebarHeight() + kNonAeroBorderThickness);
}

int PanelFrameView::TitlebarHeight() const {
  return panel::kTitlebarHeight;
}

int PanelFrameView::BorderThickness() const {
  return is_frameless_ ? 0 : kNonAeroBorderThickness;
}

PanelFrameView::PaintState PanelFrameView::GetPaintState() const {
  if (panel_view_->panel()->IsDrawingAttention())
    return PAINT_FOR_ATTENTION;
  if (bounds().height() <= panel::kMinimizedPanelHeight)
    return PAINT_AS_MINIMIZED;
  if (panel_view_->IsPanelActive() &&
           !panel_view_->force_to_paint_as_inactive())
    return PAINT_AS_ACTIVE;
  return PAINT_AS_INACTIVE;
}

SkColor PanelFrameView::GetTitleColor(PaintState paint_state) const {
  return kTitleTextDefaultColor;
}

const gfx::ImageSkia* PanelFrameView::GetFrameBackground(
    PaintState paint_state) const {
  switch (paint_state) {
    case PAINT_AS_INACTIVE:
      return GetInactiveBackgroundDefaultImage();
    case PAINT_AS_ACTIVE:
      return GetActiveBackgroundDefaultImage();
    case PAINT_AS_MINIMIZED:
      return GetMinimizeBackgroundDefaultImage();
    case PAINT_FOR_ATTENTION:
      return GetAttentionBackgroundDefaultImage();
    default:
      NOTREACHED();
      return GetInactiveBackgroundDefaultImage();
  }
}

void PanelFrameView::UpdateControlStyles(PaintState paint_state) {
  title_label_->SetEnabledColor(GetTitleColor(paint_state));
}

void PanelFrameView::PaintFrameBackground(gfx::Canvas* canvas) {
  // We only need to paint the title-bar since no resizing border is shown.
  // Instead, we allow part of the inner content area be used to trigger the
  // mouse resizing.
  int titlebar_height = TitlebarHeight();
  const gfx::ImageSkia* image = GetFrameBackground(GetPaintState());
  canvas->TileImageInt(*image, 0, 0, width(), titlebar_height);

  if (is_frameless_)
    return;

  // Left border, below title-bar.
  canvas->TileImageInt(*image, 0, titlebar_height, kNonAeroBorderThickness,
      height() - titlebar_height);

  // Right border, below title-bar.
  canvas->TileImageInt(*image, width() - kNonAeroBorderThickness,
      titlebar_height, kNonAeroBorderThickness, height() - titlebar_height);

  // Bottom border.
  canvas->TileImageInt(*image, 0, height() - kNonAeroBorderThickness, width(),
      kNonAeroBorderThickness);
}

void PanelFrameView::PaintFrameEdge(gfx::Canvas* canvas) {
#if defined(OS_WIN)
  // Border is not needed when panel is not shown as minimized.
  if (GetPaintState() != PAINT_AS_MINIMIZED)
    return;

  const gfx::ImageSkia& top_left_image = GetTopLeftCornerImage(corner_style_);
  const gfx::ImageSkia& top_right_image = GetTopRightCornerImage(corner_style_);
  const gfx::ImageSkia& bottom_left_image =
      GetBottomLeftCornerImage(corner_style_);
  const gfx::ImageSkia& bottom_right_image =
      GetBottomRightCornerImage(corner_style_);
  const gfx::ImageSkia& top_image = GetTopEdgeImage();
  const gfx::ImageSkia& bottom_image = GetBottomEdgeImage();
  const gfx::ImageSkia& left_image = GetLeftEdgeImage();
  const gfx::ImageSkia& right_image = GetRightEdgeImage();

  // Draw the top border.
  canvas->DrawImageInt(top_left_image, 0, 0);
  canvas->TileImageInt(top_image,
                       top_left_image.width(),
                       0,
                       width() - top_right_image.width(),
                       top_image.height());
  canvas->DrawImageInt(top_right_image, width() - top_right_image.width(), 0);

  // Draw the right border.
  canvas->TileImageInt(right_image,
                       width() - right_image.width(),
                       top_right_image.height(),
                       right_image.width(),
                       height() - top_right_image.height() -
                           bottom_right_image.height());

  // Draw the bottom border.
  canvas->DrawImageInt(bottom_right_image,
                       width() - bottom_right_image.width(),
                       height() - bottom_right_image.height());
  canvas->TileImageInt(bottom_image,
                       bottom_left_image.width(),
                       height() - bottom_image.height(),
                       width() - bottom_left_image.width() -
                           bottom_right_image.width(),
                       bottom_image.height());
  canvas->DrawImageInt(bottom_left_image,
                       0,
                       height() - bottom_left_image.height());

  // Draw the left border.
  canvas->TileImageInt(left_image,
                       0,
                       top_left_image.height(),
                       left_image.width(),
                       height() - top_left_image.height() -
                           bottom_left_image.height());
#endif
}
