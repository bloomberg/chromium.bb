// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/panels/panel_frame_view.h"

#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
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

#if defined(OS_WIN) && !defined(USE_AURA)
#include "ui/base/win/shell.h"
#endif

namespace {

// The thickness of the border when Aero is not enabled. In this case, the
// shadow around the window will not be painted by the system and we need to
// paint a frame in order to differentiate the client area from the background.
const int kNonAeroBorderThickness = 1;

// In the window corners, the resize areas don't actually expand bigger, but the
// 16 px at the end of each edge triggers diagonal resizing.
const int kResizeAreaCornerSize = 16;

// The spacing in pixels between the icon and the left border.
const int kIconAndBorderSpacing = 4;

// The height and width in pixels of the icon.
const int kIconSize = 16;

// The font to use to draw the title.
const char* kTitleFontName = "Arial Bold";
const int kTitleFontSize = 14;

// The spacing in pixels between the title and the icon on the left, or the
// button on the right.
const int kTitleSpacing = 11;

// The spacing in pixels between the close button and the right border.
const int kCloseButtonAndBorderSpacing = 11;

// The spacing in pixels between the close button and the minimize/restore
// button.
const int kMinimizeButtonAndCloseButtonSpacing = 5;

// Colors used to draw titlebar background under default theme.
const SkColor kActiveBackgroundDefaultColor = SkColorSetRGB(0x3a, 0x3d, 0x3d);
const SkColor kInactiveBackgroundDefaultColor = SkColorSetRGB(0x7a, 0x7c, 0x7c);
const SkColor kAttentionBackgroundDefaultColor =
    SkColorSetRGB(0xff, 0xab, 0x57);

// Color used to draw the minimized panel.
const SkColor kMinimizeBackgroundDefaultColor = SkColorSetRGB(0xf5, 0xf4, 0xf0);
const SkColor kMinimizeBorderDefaultColor = SkColorSetRGB(0xc9, 0xc9, 0xc9);

// Color used to draw the title text under default theme.
const SkColor kTitleTextDefaultColor = SkColorSetRGB(0xf9, 0xf9, 0xf9);

// Color used to draw the divider line between the titlebar and the client area.
#if defined(USE_AURA)
const SkColor kDividerColor = SkColorSetRGB(0xb5, 0xb5, 0xb5);
#else
const SkColor kDividerColor = SkColorSetRGB(0x2a, 0x2c, 0x2c);
#endif

struct EdgeResources {
  gfx::ImageSkia* top_left;
  gfx::ImageSkia* top;
  gfx::ImageSkia* top_right;
  gfx::ImageSkia* right;
  gfx::ImageSkia* bottom_right;
  gfx::ImageSkia* bottom;
  gfx::ImageSkia* bottom_left;
  gfx::ImageSkia* left;

  EdgeResources(int top_left_id, int top_id, int top_right_id, int right_id,
                int bottom_right_id, int bottom_id, int bottom_left_id,
                int left_id)
      : top_left(NULL),
        top(NULL),
        top_right(NULL),
        right(NULL),
        bottom_right(NULL),
        bottom(NULL),
        bottom_left(NULL),
        left(NULL) {
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    top_left = rb.GetImageSkiaNamed(top_left_id);
    top = rb.GetImageSkiaNamed(top_id);
    top_right = rb.GetImageSkiaNamed(top_right_id);
    right = rb.GetImageSkiaNamed(right_id);
    bottom_right = rb.GetImageSkiaNamed(bottom_right_id);
    bottom = rb.GetImageSkiaNamed(bottom_id);
    bottom_left = rb.GetImageSkiaNamed(bottom_left_id);
    left = rb.GetImageSkiaNamed(left_id);
  }
};

gfx::ImageSkia* CreateImageForColor(SkColor color) {
  gfx::Canvas canvas(gfx::Size(1, 1), ui::SCALE_FACTOR_100P, true);
  canvas.DrawColor(color);
  return new gfx::ImageSkia(canvas.ExtractImageRep());
}

const EdgeResources& GetFrameEdges() {
  static EdgeResources* edges = NULL;
  if (!edges) {
    edges = new EdgeResources(
        IDR_WINDOW_TOP_LEFT_CORNER, IDR_WINDOW_TOP_CENTER,
        IDR_WINDOW_TOP_RIGHT_CORNER, IDR_WINDOW_RIGHT_SIDE,
        IDR_PANEL_BOTTOM_RIGHT_CORNER, IDR_WINDOW_BOTTOM_CENTER,
        IDR_PANEL_BOTTOM_LEFT_CORNER, IDR_WINDOW_LEFT_SIDE);
  }
  return *edges;
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

}  // namespace

const char PanelFrameView::kViewClassName[] =
    "browser/ui/panels/PanelFrameView";

PanelFrameView::PanelFrameView(PanelView* panel_view)
    : is_frameless_(false),
      panel_view_(panel_view),
      paint_state_(NOT_PAINTED),
      close_button_(NULL),
      minimize_button_(NULL),
      restore_button_(NULL),
      title_icon_(NULL),
      title_label_(NULL) {
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
  string16 tooltip_text = l10n_util::GetStringUTF16(IDS_PANEL_CLOSE_TOOLTIP);
  close_button_->SetTooltipText(tooltip_text);
  close_button_->SetAccessibleName(tooltip_text);
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
  minimize_button_->SetAccessibleName(tooltip_text);
  AddChildView(minimize_button_);

  restore_button_ = new views::ImageButton(this);
  restore_button_->SetImage(views::CustomButton::STATE_NORMAL,
                            rb.GetImageSkiaNamed(IDR_PANEL_RESTORE));
  restore_button_->SetImage(views::CustomButton::STATE_HOVERED,
                            rb.GetImageSkiaNamed(IDR_PANEL_RESTORE_H));
  restore_button_->SetImage(views::CustomButton::STATE_PRESSED,
                            rb.GetImageSkiaNamed(IDR_PANEL_RESTORE_C));
  tooltip_text = l10n_util::GetStringUTF16(IDS_PANEL_RESTORE_TOOLTIP);
  restore_button_->SetTooltipText(tooltip_text);
  restore_button_->SetAccessibleName(tooltip_text);
  restore_button_->SetVisible(false);  // only visible when panel is minimized
  AddChildView(restore_button_);

  title_icon_ = new TabIconView(this);
  title_icon_->set_is_light(true);
  AddChildView(title_icon_);
  title_icon_->Update();

  title_label_ = new views::Label(panel_view_->panel()->GetWindowTitle());
  title_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  title_label_->SetAutoColorReadabilityEnabled(false);
  AddChildView(title_label_);
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
  minimize_button_->SetVisible(panel->CanMinimize());
  restore_button_->SetVisible(panel->CanRestore());

  // Reset the button states in case that the hover states are not cleared when
  // mouse is clicked but not moved.
  minimize_button_->SetState(views::CustomButton::STATE_NORMAL);
  restore_button_->SetState(views::CustomButton::STATE_NORMAL);
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
  int frame_component = GetHTComponentForFrame(
      point,
      PanelView::kResizeInsideBoundsSize,
      PanelView::kResizeInsideBoundsSize,
      kResizeAreaCornerSize,
      kResizeAreaCornerSize,
      resizability != panel::NOT_RESIZABLE);

  // The bottom edge and corners cannot be used to resize in some scenarios,
  // i.e docked panels.
  if (resizability == panel::RESIZABLE_ALL_SIDES_EXCEPT_BOTTOM &&
      (frame_component == HTBOTTOM ||
       frame_component == HTBOTTOMLEFT ||
       frame_component == HTBOTTOMRIGHT))
    frame_component = HTNOWHERE;

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
  window_mask->moveTo(0, 3);
  window_mask->lineTo(1, 2);
  window_mask->lineTo(1, 1);
  window_mask->lineTo(2, 1);
  window_mask->lineTo(3, 0);
  window_mask->lineTo(SkIntToScalar(size.width() - 3), 0);
  window_mask->lineTo(SkIntToScalar(size.width() - 2), 1);
  window_mask->lineTo(SkIntToScalar(size.width() - 1), 1);
  window_mask->lineTo(SkIntToScalar(size.width() - 1), 2);
  window_mask->lineTo(SkIntToScalar(size.width() - 1), 3);
  window_mask->lineTo(SkIntToScalar(size.width()),
                      SkIntToScalar(size.height()));
  window_mask->lineTo(0, SkIntToScalar(size.height()));
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
  // This makes the panel be able to shrink to very small, like minimized panel.
  return gfx::Size();
}

gfx::Size PanelFrameView::GetMaximumSize() {
  // When the user resizes the panel, there is no max size limit.
  return gfx::Size();
}

ui::ThemeProvider* PanelFrameView::GetThemeProvider() const {
  return ThemeServiceFactory::GetForProfile(panel_view_->panel()->profile());
}

void PanelFrameView::Layout() {
  // Frameless is only supported when Aero is enabled and shadow effect is
  // present.
#if defined(OS_WIN) && !defined(USE_AURA)
  is_frameless_ = ui::win::IsAeroGlassEnabled();

  if (is_frameless_) {
    BOOL shadow_enabled = FALSE;
    if (::SystemParametersInfo(SPI_GETDROPSHADOW, 0, &shadow_enabled, 0) &&
        !shadow_enabled)
      is_frameless_ = false;
  }
#endif

  // Layout the close button.
  int right = width();
  gfx::Size close_button_size = close_button_->GetPreferredSize();
  close_button_->SetBounds(
      width() - kCloseButtonAndBorderSpacing - close_button_size.width(),
      (TitlebarHeight() - close_button_size.height()) / 2,
      close_button_size.width(),
      close_button_size.height());
  right = close_button_->x();

  // Layout the minimize and restore button. Both occupy the same space,
  // but at most one is visible at any time.
  gfx::Size button_size = minimize_button_->GetPreferredSize();
  minimize_button_->SetBounds(
      right - kMinimizeButtonAndCloseButtonSpacing - button_size.width(),
      (TitlebarHeight() - button_size.height()) / 2,
      button_size.width(),
      button_size.height());
  restore_button_->SetBoundsRect(minimize_button_->bounds());
  right = minimize_button_->x();

  // Layout the icon.
  int icon_y = (TitlebarHeight() - kIconSize) / 2;
  title_icon_->SetBounds(
      kIconAndBorderSpacing,
      icon_y,
      kIconSize,
      kIconSize);

  // Layout the title.
  int title_x = title_icon_->bounds().right() + kTitleSpacing;
  int title_height = GetTitleFont().GetHeight();
  title_label_->SetBounds(
      title_x,
      icon_y + ((kIconSize - title_height - 1) / 2),
      std::max(0, right - kTitleSpacing - title_x),
      title_height);
}

void PanelFrameView::OnPaint(gfx::Canvas* canvas) {
  // The font and color need to be updated depending on the panel's state.
  PaintState paint_state;
  if (panel_view_->panel()->IsDrawingAttention())
    paint_state = PAINT_FOR_ATTENTION;
  else if (bounds().height() <= panel::kMinimizedPanelHeight)
    paint_state = PAINT_AS_MINIMIZED;
  else if (panel_view_->IsPanelActive() &&
           !panel_view_->force_to_paint_as_inactive())
    paint_state = PAINT_AS_ACTIVE;
  else
    paint_state = PAINT_AS_INACTIVE;

  UpdateControlStyles(paint_state);
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

bool PanelFrameView::UsingDefaultTheme(PaintState paint_state) const {
  // No theme is provided for attention painting.
  if (paint_state == PAINT_FOR_ATTENTION)
    return true;

  ThemeService* theme_service = ThemeServiceFactory::GetForProfile(
      panel_view_->panel()->profile());
  return theme_service->UsingDefaultTheme();
}

SkColor PanelFrameView::GetTitleColor(PaintState paint_state) const {
  return UsingDefaultTheme(paint_state) ?
      GetDefaultTitleColor(paint_state) :
      GetThemedTitleColor(paint_state);
}

SkColor PanelFrameView::GetDefaultTitleColor(
    PaintState paint_state) const {
  return kTitleTextDefaultColor;
}

SkColor PanelFrameView::GetThemedTitleColor(
    PaintState paint_state) const {
  return GetThemeProvider()->GetColor(paint_state == PAINT_AS_ACTIVE ?
      ThemeService::COLOR_TAB_TEXT : ThemeService::COLOR_BACKGROUND_TAB_TEXT);
}

const gfx::ImageSkia* PanelFrameView::GetFrameBackground(
    PaintState paint_state) const {
  return UsingDefaultTheme(paint_state) ?
      GetDefaultFrameBackground(paint_state) :
      GetThemedFrameBackground(paint_state);
}

const gfx::ImageSkia* PanelFrameView::GetDefaultFrameBackground(
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

const gfx::ImageSkia* PanelFrameView::GetThemedFrameBackground(
    PaintState paint_state) const {
  return GetThemeProvider()->GetImageSkiaNamed(paint_state == PAINT_AS_ACTIVE ?
      IDR_THEME_TOOLBAR : IDR_THEME_TAB_BACKGROUND);
}

void PanelFrameView::UpdateControlStyles(PaintState paint_state) {
  DCHECK(paint_state != NOT_PAINTED);

  if (paint_state == paint_state_)
    return;
  paint_state_ = paint_state;

  SkColor title_color = GetTitleColor(paint_state_);
  title_label_->SetEnabledColor(title_color);
  title_label_->SetFont(GetTitleFont());
}

void PanelFrameView::PaintFrameBackground(gfx::Canvas* canvas) {
  // We only need to paint the title-bar since no resizing border is shown.
  // Instead, we allow part of the inner content area be used to trigger the
  // mouse resizing.
  int titlebar_height = TitlebarHeight();
  const gfx::ImageSkia* image = GetFrameBackground(paint_state_);
  canvas->TileImageInt(*image, 0, 0, width(), titlebar_height);

  if (is_frameless_)
    return;

  // For all the non-client area below titlebar, we paint it by using the last
  // line from the theme image, instead of using the frame color provided
  // in the theme because the frame color in some themes is very different from
  // the tab colors we use to render the titlebar and it can produce abrupt
  // transition which looks bad.

  // Left border, below title-bar.
  canvas->DrawImageInt(
      *image, 0, titlebar_height - 1, kNonAeroBorderThickness, 1,
      0, titlebar_height, kNonAeroBorderThickness, height() - titlebar_height,
      false);

  // Right border, below title-bar.
  canvas->DrawImageInt(
      *image, (width() % image->width()) - kNonAeroBorderThickness,
      titlebar_height - 1, kNonAeroBorderThickness, 1,
      width() - kNonAeroBorderThickness, titlebar_height,
      kNonAeroBorderThickness, height() - titlebar_height, false);

  // Bottom border.
  canvas->DrawImageInt(
      *image, 0, titlebar_height - 1, image->width(), 1,
      0, height() - kNonAeroBorderThickness, width(), kNonAeroBorderThickness,
      false);
}

void PanelFrameView::PaintFrameEdge(gfx::Canvas* canvas) {
#if !defined(USE_AURA)
  // Border is not needed when panel is not shown as minimized.
  if (paint_state_ != PAINT_AS_MINIMIZED)
    return;

  // Draw the top border.
  const EdgeResources& frame_edges = GetFrameEdges();
  canvas->DrawImageInt(*(frame_edges.top_left), 0, 0);
  canvas->TileImageInt(
      *(frame_edges.top), frame_edges.top_left->width(), 0,
      width() - frame_edges.top_right->width(), frame_edges.top->height());
  canvas->DrawImageInt(
      *(frame_edges.top_right),
      width() - frame_edges.top_right->width(), 0);

  // Draw the right border.
  canvas->TileImageInt(
      *(frame_edges.right), width() - frame_edges.right->width(),
      frame_edges.top_right->height(), frame_edges.right->width(),
      height() - frame_edges.top_right->height() -
          frame_edges.bottom_right->height());

  // Draw the bottom border.
  canvas->DrawImageInt(
      *(frame_edges.bottom_right),
      width() - frame_edges.bottom_right->width(),
      height() - frame_edges.bottom_right->height());
  canvas->TileImageInt(
      *(frame_edges.bottom), frame_edges.bottom_left->width(),
      height() - frame_edges.bottom->height(),
      width() - frame_edges.bottom_left->width() -
          frame_edges.bottom_right->width(),
      frame_edges.bottom->height());
  canvas->DrawImageInt(
      *(frame_edges.bottom_left), 0,
      height() - frame_edges.bottom_left->height());

  // Draw the left border.
  canvas->TileImageInt(
      *(frame_edges.left), 0, frame_edges.top_left->height(),
      frame_edges.left->width(),
      height() - frame_edges.top_left->height() -
          frame_edges.bottom_left->height());
#endif
}
