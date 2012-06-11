// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/panels/panel_browser_frame_view.h"

#include <algorithm>

#include "base/utf_string_conversions.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/panels/panel.h"
#include "chrome/browser/ui/panels/panel_browser_view.h"
#include "chrome/browser/ui/panels/panel_manager.h"
#include "chrome/browser/ui/panels/panel_strip.h"
#include "chrome/common/extensions/extension.h"
#include "content/public/browser/web_contents.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "grit/theme_resources_standard.h"
#include "grit/ui_resources.h"
#include "ui/base/accessibility/accessible_view_state.h"
#include "ui/base/hit_test.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/path.h"
#include "ui/gfx/screen.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/widget/widget_delegate.h"

#if defined(USE_AURA) && defined(USE_X11)
#include "ui/base/x/x11_util.h"
#endif

using content::WebContents;

namespace {

// The thickness in pixels of the border.
#if defined(USE_AURA)
// No border inside the window frame; see comment in PaintFrameBorder().
const int kResizableBorderThickness = 0;
const int kNonResizableBorderThickness = 0;
#else
const int kResizableBorderThickness = 4;
const int kNonResizableBorderThickness = 1;
#endif

// In the window corners, the resize areas don't actually expand bigger, but the
// 16 px at the end of each edge triggers diagonal resizing.
const int kResizeAreaCornerSize = 16;

// Colors used to draw client edges. These are experimental values.
const SkColor kClientEdgeColor = SkColorSetRGB(210, 225, 246);
const SkColor kContentsBorderShadow = SkColorSetARGB(51, 0, 0, 0);

// The spacing in pixels between the icon and the left border.
const int kIconAndBorderSpacing = 4;

// The height and width in pixels of the icon.
const int kIconSize = 16;

// The font to use to draw the title.
const char* kTitleFontName = "Arial";
const int kTitleFontSize = 14;

// The spacing in pixels between the title and the icon on the left, or the
// button on the right.
const int kTitleSpacing = 13;

// The spacing in pixels between the close button and the right border.
const int kCloseButtonAndBorderSpacing = 13;

// The spacing in pixels between the close button and the minimize/restore
// button.
const int kMinimizeButtonAndCloseButtonSpacing = 13;

// Colors used to draw titlebar background under default theme.
const SkColor kActiveBackgroundDefaultColor = SkColorSetRGB(0x3a, 0x3c, 0x3c);
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
const SkColor kDividerColor = SkColorSetRGB(0x5a, 0x5c, 0x5c);
#endif

struct ButtonResources {
  gfx::ImageSkia* normal_image;
  gfx::ImageSkia* hover_image;
  gfx::ImageSkia* push_image;
  string16 tooltip_text;

  ButtonResources(int normal_image_id, int hover_image_id, int push_image_id,
                  int tooltip_id)
      : normal_image(NULL),
        hover_image(NULL),
        push_image(NULL) {
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    normal_image = rb.GetImageSkiaNamed(normal_image_id);
    hover_image = rb.GetImageSkiaNamed(hover_image_id);
    push_image = rb.GetImageSkiaNamed(push_image_id);
    tooltip_text = l10n_util::GetStringUTF16(tooltip_id);
  }
};

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
  gfx::Canvas canvas(gfx::Size(1, 1), true);
  canvas.DrawColor(color);
  return new gfx::ImageSkia(canvas.ExtractBitmap());
}

const ButtonResources& GetCloseButtonResources() {
  static ButtonResources* buttons = NULL;
  if (!buttons) {
    buttons = new ButtonResources(IDR_PANEL_CLOSE,
                                  IDR_PANEL_CLOSE_H,
                                  IDR_PANEL_CLOSE_C,
                                  IDS_PANEL_CLOSE_TOOLTIP);
  }
  return *buttons;
}

const ButtonResources& GetMinimizeButtonResources() {
  static ButtonResources* buttons = NULL;
  if (!buttons) {
    buttons = new ButtonResources(IDR_PANEL_MINIMIZE,
                                  IDR_PANEL_MINIMIZE_H,
                                  IDR_PANEL_MINIMIZE_C,
                                  IDS_PANEL_MINIMIZE_TOOLTIP);
  }
  return *buttons;
}

const ButtonResources& GetRestoreButtonResources() {
  static ButtonResources* buttons = NULL;
  if (!buttons) {
    buttons = new ButtonResources(IDR_PANEL_RESTORE,
                                  IDR_PANEL_RESTORE_H,
                                  IDR_PANEL_RESTORE_C,
                                  IDS_PANEL_RESTORE_TOOLTIP);
  }
  return *buttons;
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

const EdgeResources& GetClientEdges() {
  static EdgeResources* edges = NULL;
  if (!edges) {
    edges = new EdgeResources(
        IDR_APP_TOP_LEFT, IDR_APP_TOP_CENTER,
        IDR_APP_TOP_RIGHT, IDR_CONTENT_RIGHT_SIDE,
        IDR_CONTENT_BOTTOM_RIGHT_CORNER, IDR_CONTENT_BOTTOM_CENTER,
        IDR_CONTENT_BOTTOM_LEFT_CORNER, IDR_CONTENT_LEFT_SIDE);
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

bool IsHitTestValueForResizing(int hc) {
  return hc == HTLEFT || hc == HTRIGHT || hc == HTTOP || hc == HTBOTTOM ||
         hc == HTTOPLEFT || hc == HTTOPRIGHT || hc == HTBOTTOMLEFT ||
         hc == HTBOTTOMRIGHT;
}

}  // namespace

PanelBrowserFrameView::PanelBrowserFrameView(BrowserFrame* frame,
                                             PanelBrowserView* browser_view)
    : BrowserNonClientFrameView(frame, browser_view),
      panel_browser_view_(browser_view),
      paint_state_(NOT_PAINTED),
      close_button_(NULL),
      minimize_button_(NULL),
      restore_button_(NULL),
      title_icon_(NULL),
      title_label_(NULL) {
  frame->set_frame_type(views::Widget::FRAME_TYPE_FORCE_CUSTOM);

  const ButtonResources& close_button_resources = GetCloseButtonResources();
  close_button_ = new views::ImageButton(this);
  close_button_->SetImage(views::CustomButton::BS_NORMAL,
                          close_button_resources.normal_image);
  close_button_->SetImage(views::CustomButton::BS_HOT,
                          close_button_resources.hover_image);
  close_button_->SetImage(views::CustomButton::BS_PUSHED,
                          close_button_resources.push_image);
  close_button_->SetTooltipText(close_button_resources.tooltip_text);
  close_button_->SetAccessibleName(close_button_resources.tooltip_text);
  AddChildView(close_button_);

  const ButtonResources& minimize_button_resources =
      GetMinimizeButtonResources();
  minimize_button_ = new views::ImageButton(this);
  minimize_button_->SetImage(views::CustomButton::BS_NORMAL,
                             minimize_button_resources.normal_image);
  minimize_button_->SetImage(views::CustomButton::BS_HOT,
                             minimize_button_resources.hover_image);
  minimize_button_->SetImage(views::CustomButton::BS_PUSHED,
                             minimize_button_resources.push_image);
  minimize_button_->SetTooltipText(minimize_button_resources.tooltip_text);
  minimize_button_->SetAccessibleName(minimize_button_resources.tooltip_text);
  AddChildView(minimize_button_);

  const ButtonResources& restore_button_resources =
      GetRestoreButtonResources();
  restore_button_ = new views::ImageButton(this);
  restore_button_->SetImage(views::CustomButton::BS_NORMAL,
                            restore_button_resources.normal_image);
  restore_button_->SetImage(views::CustomButton::BS_HOT,
                            restore_button_resources.hover_image);
  restore_button_->SetImage(views::CustomButton::BS_PUSHED,
                            restore_button_resources.push_image);
  restore_button_->SetTooltipText(restore_button_resources.tooltip_text);
  restore_button_->SetAccessibleName(restore_button_resources.tooltip_text);
  restore_button_->SetVisible(false);  // only visible when panel is minimized
  AddChildView(restore_button_);

  title_icon_ = new TabIconView(this);
  title_icon_->set_is_light(true);
  AddChildView(title_icon_);
  title_icon_->Update();

  title_label_ = new views::Label(GetTitleText());
  title_label_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  title_label_->SetAutoColorReadabilityEnabled(false);
  AddChildView(title_label_);
}

PanelBrowserFrameView::~PanelBrowserFrameView() {
}

gfx::Rect PanelBrowserFrameView::GetBoundsForTabStrip(
    views::View* tabstrip) const {
  // Panels never show a tab strip.
  NOTREACHED();
  return gfx::Rect();
}

int PanelBrowserFrameView::GetHorizontalTabStripVerticalOffset(
    bool restored) const {
  // This is not needed since we do not show tab strip for the panel.
  return 0;
}

void PanelBrowserFrameView::UpdateThrobber(bool running) {
  // Tells the title icon to update the throbber when we need to show the
  // animation to indicate we're still loading.
  title_icon_->Update();
}

gfx::Rect PanelBrowserFrameView::GetBoundsForClientView() const {
  return client_view_bounds_;
}

gfx::Rect PanelBrowserFrameView::GetWindowBoundsForClientBounds(
    const gfx::Rect& client_bounds) const {
  int top_height = NonClientTopBorderHeight();
  int border_thickness = NonClientBorderThickness();
  return gfx::Rect(std::max(0, client_bounds.x() - border_thickness),
                   std::max(0, client_bounds.y() - top_height),
                   client_bounds.width() + (2 * border_thickness),
                   client_bounds.height() + top_height + border_thickness);
}

int PanelBrowserFrameView::NonClientHitTest(const gfx::Point& point) {
  PanelStrip* panel_strip = panel_browser_view_->panel()->panel_strip();
  if (!panel_strip)
    return HTNOWHERE;

  if (!bounds().Contains(point))
    return HTNOWHERE;

  int frame_component =
      frame()->client_view()->NonClientHitTest(point);
  if (frame_component != HTNOWHERE)
    return frame_component;

  if (close_button_->visible() &&
      close_button_->GetMirroredBounds().Contains(point))
    return HTCLOSE;

  if (minimize_button_->visible() &&
      minimize_button_->GetMirroredBounds().Contains(point))
    return HTMINBUTTON;

  if (restore_button_->visible() &&
      restore_button_->GetMirroredBounds().Contains(point))
    return HTMAXBUTTON;

  int window_component = GetHTComponentForFrame(point,
      NonClientBorderThickness(), NonClientBorderThickness(),
      kResizeAreaCornerSize, kResizeAreaCornerSize,
      CanResize());

  // The bottom edge and corners cannot be used to resize in some scenarios,
  // i.e docked panels.
  panel::Resizability resizability = panel_strip->GetPanelResizability(
      panel_browser_view_->panel());
  if (resizability == panel::RESIZABLE_ALL_SIDES_EXCEPT_BOTTOM &&
      (window_component == HTBOTTOM ||
       window_component == HTBOTTOMLEFT ||
       window_component == HTBOTTOMRIGHT))
    return HTNOWHERE;

  // Fall back to the caption if no other component matches.
  return (window_component == HTNOWHERE) ? HTCAPTION : window_component;
}

void PanelBrowserFrameView::GetWindowMask(const gfx::Size& size,
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

void PanelBrowserFrameView::ResetWindowControls() {
  // The close button isn't affected by this constraint.
}

void PanelBrowserFrameView::UpdateWindowIcon() {
  title_icon_->SchedulePaint();
}

void PanelBrowserFrameView::OnPaint(gfx::Canvas* canvas) {
  // The font and color need to be updated depending on the panel's state.
  PaintState paint_state;
  if (panel_browser_view_->panel()->IsDrawingAttention())
    paint_state = PAINT_FOR_ATTENTION;
  else if (bounds().height() <= panel::kMinimizedPanelHeight)
    paint_state = PAINT_AS_MINIMIZED;
  else if (panel_browser_view_->focused() &&
           !panel_browser_view_->force_to_paint_as_inactive())
    paint_state = PAINT_AS_ACTIVE;
  else
    paint_state = PAINT_AS_INACTIVE;

  UpdateControlStyles(paint_state);
  PaintFrameBackground(canvas);
  PaintFrameEdge(canvas);
  PaintDivider(canvas);
}

void PanelBrowserFrameView::OnThemeChanged() {
}

gfx::Size PanelBrowserFrameView::GetMinimumSize() {
  // This makes the panel be able to shrink to very small, like minimized panel.
  return gfx::Size();
}

gfx::Size PanelBrowserFrameView::GetMaximumSize() {
  // When the user resizes the panel, there is no max size limit.
  return gfx::Size();
}

void PanelBrowserFrameView::Layout() {
  Panel* panel = panel_browser_view_->panel();
  PanelStrip* panel_strip = panel->panel_strip();
  if (!panel_strip)
    return;

  // Layout the close button.
  int right = width();
  gfx::Size close_button_size = close_button_->GetPreferredSize();
  close_button_->SetBounds(
      width() - NonClientBorderThickness() - kCloseButtonAndBorderSpacing -
      close_button_size.width(),
      (NonClientTopBorderHeight() - close_button_size.height()) / 2,
      close_button_size.width(),
      close_button_size.height());
  right = close_button_->x();

  // Layout the minimize and restore button. Both occupy the same space,
  // but at most one is visible at any time.
  gfx::Size button_size = minimize_button_->GetPreferredSize();
  minimize_button_->SetBounds(
      right - kMinimizeButtonAndCloseButtonSpacing - button_size.width(),
      (NonClientTopBorderHeight() - button_size.height()) / 2,
      button_size.width(),
      button_size.height());
  restore_button_->SetBoundsRect(minimize_button_->bounds());
  right = minimize_button_->x();

  // Layout the icon.
  int icon_y = (NonClientTopBorderHeight() - kIconSize) / 2;
  title_icon_->SetBounds(
      NonClientBorderThickness() + kIconAndBorderSpacing,
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

  // Calculate the client area bounds.
  int top_height = NonClientTopBorderHeight();
  int border_thickness = NonClientBorderThickness();
  client_view_bounds_.SetRect(
      border_thickness,
      top_height,
      std::max(0, width() - (2 * border_thickness)),
      std::max(0, height() - top_height - border_thickness));
}

void PanelBrowserFrameView::GetAccessibleState(ui::AccessibleViewState* state) {
  state->role = ui::AccessibilityTypes::ROLE_TITLEBAR;
}

bool PanelBrowserFrameView::OnMousePressed(const views::MouseEvent& event) {
  gfx::Point mouse_location = event.location();

  if (CanResize() &&
      IsHitTestValueForResizing(NonClientHitTest(mouse_location)))
    return BrowserNonClientFrameView::OnMousePressed(event);

  // |event.location| is in the view's coordinate system. Convert it to the
  // screen coordinate system.
  views::View::ConvertPointToScreen(this, &mouse_location);

  if (event.IsOnlyLeftMouseButton() &&
      panel_browser_view_->OnTitlebarMousePressed(mouse_location)) {
    return true;
  }
  return BrowserNonClientFrameView::OnMousePressed(event);
}

bool PanelBrowserFrameView::OnMouseDragged(const views::MouseEvent& event) {
  // |event.location| is in the view's coordinate system. Convert it to the
  // screen coordinate system.
  gfx::Point mouse_location = event.location();
  views::View::ConvertPointToScreen(this, &mouse_location);

  if (panel_browser_view_->OnTitlebarMouseDragged(mouse_location))
    return true;
  return BrowserNonClientFrameView::OnMouseDragged(event);
}

void PanelBrowserFrameView::OnMouseReleased(const views::MouseEvent& event) {
  if (panel_browser_view_->OnTitlebarMouseReleased(
          event.IsControlDown() ? panel::APPLY_TO_ALL : panel::NO_MODIFIER))
    return;
  BrowserNonClientFrameView::OnMouseReleased(event);
}

void PanelBrowserFrameView::OnMouseCaptureLost() {
  if (panel_browser_view_->OnTitlebarMouseCaptureLost())
    return;
  BrowserNonClientFrameView::OnMouseCaptureLost();
}

void PanelBrowserFrameView::ButtonPressed(views::Button* sender,
                                          const views::Event& event) {
  if (sender == close_button_) {
    frame()->Close();
  } else {
    panel::ClickModifier modifier =
        event.IsControlDown() ? panel::APPLY_TO_ALL : panel::NO_MODIFIER;
    if (sender == minimize_button_)
      panel_browser_view_->panel()->OnMinimizeButtonClicked(modifier);
    else if (sender == restore_button_)
      panel_browser_view_->panel()->OnRestoreButtonClicked(modifier);
  }
}

bool PanelBrowserFrameView::ShouldTabIconViewAnimate() const {
  // This function is queried during the creation of the window as the
  // TabIconView we host is initialized, so we need to NULL check the selected
  // WebContents because in this condition there is not yet a selected tab.
  WebContents* current_tab = browser_view()->GetActiveWebContents();
  return current_tab ? current_tab->IsLoading() : false;
}

gfx::ImageSkia PanelBrowserFrameView::GetFaviconForTabIconView() {
  return frame()->widget_delegate()->GetWindowIcon();
}

int PanelBrowserFrameView::NonClientBorderThickness() const {
  if (CanResize())
    return kResizableBorderThickness;
  else
    return kNonResizableBorderThickness;
}

int PanelBrowserFrameView::NonClientTopBorderHeight() const {
  return panel::kTitlebarHeight;
}

gfx::Size PanelBrowserFrameView::NonClientAreaSize() const {
  return gfx::Size(NonClientBorderThickness() * 2,
                   NonClientTopBorderHeight() + NonClientBorderThickness());
}

int PanelBrowserFrameView::IconOnlyWidth() const {
  return NonClientBorderThickness() * 2 + kIconAndBorderSpacing * 2 + kIconSize;
}

bool PanelBrowserFrameView::UsingDefaultTheme(PaintState paint_state) const {
  // No theme is provided for attention painting.
  if (paint_state == PAINT_FOR_ATTENTION)
    return true;

  ThemeService* theme_service = ThemeServiceFactory::GetForProfile(
      panel_browser_view_->panel()->profile());
  return theme_service->UsingDefaultTheme();
}

SkColor PanelBrowserFrameView::GetTitleColor(PaintState paint_state) const {
  return UsingDefaultTheme(paint_state) ?
      GetDefaultTitleColor(paint_state) :
      GetThemedTitleColor(paint_state);
}

SkColor PanelBrowserFrameView::GetDefaultTitleColor(
    PaintState paint_state) const {
  return kTitleTextDefaultColor;
}

SkColor PanelBrowserFrameView::GetThemedTitleColor(
    PaintState paint_state) const {
  return GetThemeProvider()->GetColor(paint_state == PAINT_AS_ACTIVE ?
      ThemeService::COLOR_TAB_TEXT : ThemeService::COLOR_BACKGROUND_TAB_TEXT);
}

const gfx::ImageSkia* PanelBrowserFrameView::GetFrameBackground(
    PaintState paint_state) const {
  return UsingDefaultTheme(paint_state) ?
      GetDefaultFrameBackground(paint_state) :
      GetThemedFrameBackground(paint_state);
}

const gfx::ImageSkia* PanelBrowserFrameView::GetDefaultFrameBackground(
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

const gfx::ImageSkia* PanelBrowserFrameView::GetThemedFrameBackground(
    PaintState paint_state) const {
  return GetThemeProvider()->GetImageSkiaNamed(paint_state == PAINT_AS_ACTIVE ?
      IDR_THEME_TOOLBAR : IDR_THEME_TAB_BACKGROUND);
}

void PanelBrowserFrameView::UpdateControlStyles(PaintState paint_state) {
  DCHECK(paint_state != NOT_PAINTED);

  if (paint_state == paint_state_)
    return;
  paint_state_ = paint_state;

  SkColor title_color = GetTitleColor(paint_state_);
  title_label_->SetEnabledColor(title_color);
  title_label_->SetFont(GetTitleFont());

  close_button_->SetBackground(title_color,
                               GetCloseButtonResources().normal_image,
                               NULL);

  minimize_button_->SetBackground(title_color,
                                  GetMinimizeButtonResources().normal_image,
                                  NULL);

  restore_button_->SetBackground(title_color,
                                 GetRestoreButtonResources().normal_image,
                                 NULL);
}

void PanelBrowserFrameView::PaintFrameBackground(gfx::Canvas* canvas) {
  int top_area_height = NonClientTopBorderHeight();
  int thickness = NonClientBorderThickness();
  const gfx::ImageSkia* image = GetFrameBackground(paint_state_);

  // Top area, including title-bar.
  canvas->TileImageInt(*image, 0, 0, width(), top_area_height);

  // For all the non-client area below titlebar, we paint it by using the last
  // line from the theme image, instead of using the frame color provided
  // in the theme because the frame color in some themes is very different from
  // the tab colors we use to render the titlebar and it can produce abrupt
  // transition which looks bad.

  // Left border, below title-bar.
  canvas->DrawImageInt(*image, 0, top_area_height - 1, thickness, 1,
      0, top_area_height, thickness, height() - top_area_height, false);

  // Right border, below title-bar.
  canvas->DrawImageInt(*image, (width() % image->width()) - thickness,
      top_area_height - 1, thickness, 1,
      width() - thickness, top_area_height,
      thickness, height() - top_area_height, false);

  // Bottom border.
  canvas->DrawImageInt(*image, 0, top_area_height - 1, image->width(), 1,
      0, height() - thickness, width(), thickness, false);
}

void PanelBrowserFrameView::PaintFrameEdge(gfx::Canvas* canvas) {
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
#endif  // !defined(USE_AURA)
}

void PanelBrowserFrameView::PaintDivider(gfx::Canvas* canvas) {
#if defined(USE_AURA)
  // Aura recognizes aura::client::WINDOW_TYPE_PANEL and will draw the
  // appropriate frame and shadow. See ash/wm/shadow_controller.h.

  // Draw the divider between the titlebar and the client area.
  if (!IsShowingTitlebarOnly()) {
    canvas->DrawRect(gfx::Rect(0, panel::kTitlebarHeight, width() - 1, 1),
                     kDividerColor);
  }
#else
  // Draw the divider between the titlebar and the client area.
  if (!IsShowingTitlebarOnly()) {
    canvas->DrawLine(gfx::Point(0, panel::kTitlebarHeight - 1),
                     gfx::Point(width() - 1, panel::kTitlebarHeight - 1),
                     kDividerColor);
  }
#endif  // !defined(USE_AURA)
}

string16 PanelBrowserFrameView::GetTitleText() const {
  return frame()->widget_delegate()->GetWindowTitle();
}

void PanelBrowserFrameView::UpdateTitleBar() {
  title_label_->SetText(GetTitleText());
}

void PanelBrowserFrameView::UpdateTitleBarMinimizeRestoreButtonVisibility() {
  Panel* panel = panel_browser_view_->panel();
  minimize_button_->SetVisible(panel->CanMinimize());
  restore_button_->SetVisible(panel->CanRestore());

  // Reset the button states in case that the hover states are not cleared when
  // mouse is clicked but not moved.
  minimize_button_->SetState(views::CustomButton::BS_NORMAL);
  restore_button_->SetState(views::CustomButton::BS_NORMAL);
}

bool PanelBrowserFrameView::CanResize() const {
  return panel_browser_view_->panel()->CanResizeByMouse() !=
      panel::NOT_RESIZABLE;
}

bool PanelBrowserFrameView::IsShowingTitlebarOnly() const {
  return height() <= panel::kTitlebarHeight;
}
