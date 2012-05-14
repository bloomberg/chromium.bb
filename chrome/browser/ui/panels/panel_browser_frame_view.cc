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
#include "third_party/skia/include/effects/SkGradientShader.h"
#include "ui/base/accessibility/accessible_view_state.h"
#include "ui/base/hit_test.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/path.h"
#include "ui/gfx/screen.h"
#include "ui/gfx/skia_util.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/painter.h"
#include "ui/views/widget/widget_delegate.h"

#if defined(USE_AURA) && defined(USE_X11)
#include "ui/base/x/x11_util.h"
#endif

using content::WebContents;

namespace {

// The height in pixels of the titlebar.
const int kTitlebarHeight = 24;

// The thickness in pixels of the border.
#if defined(USE_AURA)
// No border inside the window frame; see comment in PaintFrameBorder().
const int kResizableBorderThickness = 0;
const int kNonResizableBorderThickness = 0;
#else
const int kResizableBorderThickness = 4;
const int kNonResizableBorderThickness = 1;
#endif

// Client edge is only present on thick border when the panel is resizable.
const int kResizableClientEdgeThickness = 1;
const int kNonResizableClientEdgeThickness = 0;

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

// The spacing in pixels between the title and the icon on the left, or the
// button on the right.
const int kTitleSpacing = 8;

// The spacing in pixels between the close button and the right border.
const int kCloseButtonAndBorderSpacing = 8;

// The spacing in pixels between the close button and the minimize/restore
// button.
const int kMinimizeButtonAndCloseButtonSpacing = 8;

// Colors used to draw active titlebar under default theme.
const SkColor kActiveTitleTextDefaultColor = SK_ColorBLACK;
const SkColor kActiveBackgroundDefaultColorStart = 0xfff0f8fa;
const SkColor kActiveBackgroundDefaultColorEnd = 0xffc1d2dd;

// Colors used to draw inactive titlebar under default theme.
const SkColor kInactiveTitleTextDefaultColor = 0x80888888;
const SkColor kInactiveBackgroundDefaultColorStart = 0xffffffff;
const SkColor kInactiveBackgroundDefaultColorEnd = 0xffe7edf1;

// Alpha value used in drawing inactive titlebar under default theme.
const U8CPU kInactiveAlphaBlending = 0x80;

// Colors used to draw titlebar for drawing attention under default theme.
// It is also used in non-default theme since attention color is not defined
// in the theme.
const SkColor kAttentionTitleTextDefaultColor = SK_ColorWHITE;
const SkColor kAttentionBackgroundDefaultColorStart = 0xffffab57;
const SkColor kAttentionBackgroundDefaultColorEnd = 0xfff59338;

// Color used to draw the border.
const SkColor kBorderColor = 0xc0000000;

// Color used to draw the divider line between the titlebar and the client area.
const SkColor kDividerColor = 0xffb5b5b5;

struct ButtonResources {
  SkBitmap* normal_image;
  SkBitmap* hover_image;
  string16 tooltip_text;

  ButtonResources(int normal_image_id, int hover_image_id, int tooltip_id)
      : normal_image(NULL),
        hover_image(NULL) {
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    normal_image = rb.GetBitmapNamed(normal_image_id);
    hover_image = rb.GetBitmapNamed(hover_image_id);
    tooltip_text = l10n_util::GetStringUTF16(tooltip_id);
  }
};

struct EdgeResources {
  SkBitmap* top_left;
  SkBitmap* top;
  SkBitmap* top_right;
  SkBitmap* right;
  SkBitmap* bottom_right;
  SkBitmap* bottom;
  SkBitmap* bottom_left;
  SkBitmap* left;

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
    top_left = rb.GetBitmapNamed(top_left_id);
    top = rb.GetBitmapNamed(top_id);
    top_right = rb.GetBitmapNamed(top_right_id);
    right = rb.GetBitmapNamed(right_id);
    bottom_right = rb.GetBitmapNamed(bottom_right_id);
    bottom = rb.GetBitmapNamed(bottom_id);
    bottom_left = rb.GetBitmapNamed(bottom_left_id);
    left = rb.GetBitmapNamed(left_id);
  }
};

SkBitmap* CreateGradientBitmap(SkColor start_color, SkColor end_color) {
  int gradient_size = kResizableBorderThickness +
      kResizableClientEdgeThickness + kTitlebarHeight;
  SkShader* shader = gfx::CreateGradientShader(
      0, gradient_size, start_color, end_color);
  SkPaint paint;
  paint.setStyle(SkPaint::kFill_Style);
  paint.setAntiAlias(true);
  paint.setShader(shader);
  shader->unref();
  gfx::Canvas canvas(gfx::Size(1, gradient_size), true);
  canvas.DrawRect(gfx::Rect(0, 0, 1, gradient_size), paint);
  return new SkBitmap(canvas.ExtractBitmap());
}

const ButtonResources& GetCloseButtonResources() {
  static ButtonResources* buttons = NULL;
  if (!buttons) {
    buttons = new ButtonResources(IDR_PANEL_CLOSE,
                                  IDR_PANEL_CLOSE_H,
                                  IDS_PANEL_CLOSE_TOOLTIP);
  }
  return *buttons;
}

const ButtonResources& GetMinimizeButtonResources() {
  static ButtonResources* buttons = NULL;
  if (!buttons) {
    buttons = new ButtonResources(IDR_PANEL_MINIMIZE,
                                  IDR_PANEL_MINIMIZE_H,
                                  IDS_PANEL_MINIMIZE_TOOLTIP);
  }
  return *buttons;
}

const ButtonResources& GetRestoreButtonResources() {
  static ButtonResources* buttons = NULL;
  if (!buttons) {
    buttons = new ButtonResources(IDR_PANEL_RESTORE,
                                  IDR_PANEL_RESTORE_H,
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
  if (!font) {
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    font = new gfx::Font(rb.GetFont(ResourceBundle::BoldFont));
  }
  return *font;
}

SkBitmap* GetActiveBackgroundDefaultBitmap() {
  static SkBitmap* bitmap = NULL;
  if (!bitmap) {
    bitmap = CreateGradientBitmap(kActiveBackgroundDefaultColorStart,
                                  kActiveBackgroundDefaultColorEnd);
  }
  return bitmap;
}

SkBitmap* GetInactiveBackgroundDefaultBitmap() {
  static SkBitmap* bitmap = NULL;
  if (!bitmap) {
    bitmap = CreateGradientBitmap(kInactiveBackgroundDefaultColorStart,
                                  kInactiveBackgroundDefaultColorEnd);
  }
  return bitmap;
}

SkBitmap* GetAttentionBackgroundDefaultBitmap() {
  static SkBitmap* bitmap = NULL;
  if (!bitmap) {
    bitmap = CreateGradientBitmap(kAttentionBackgroundDefaultColorStart,
                                  kAttentionBackgroundDefaultColorEnd);
  }
  return bitmap;
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
  else if (panel_browser_view_->focused())
    paint_state = PAINT_AS_ACTIVE;
  else
    paint_state = PAINT_AS_INACTIVE;

  UpdateControlStyles(paint_state);
  PaintFrameBackground(canvas);
  PaintFrameEdge(canvas);
  PaintClientEdge(canvas);
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
  int title_height = BrowserFrame::GetTitleFont().GetHeight();
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
  WebContents* current_tab = browser_view()->GetSelectedWebContents();
  return current_tab ? current_tab->IsLoading() : false;
}

SkBitmap PanelBrowserFrameView::GetFaviconForTabIconView() {
  return frame()->widget_delegate()->GetWindowIcon();
}

int PanelBrowserFrameView::NonClientBorderThickness() const {
  if (CanResize())
    return kResizableBorderThickness + kResizableClientEdgeThickness;
  else
    return kNonResizableBorderThickness + kNonResizableClientEdgeThickness;
}

int PanelBrowserFrameView::NonClientTopBorderHeight() const {
  return NonClientBorderThickness() + kTitlebarHeight;
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
      panel_browser_view_->panel()->browser()->profile());
  return theme_service->UsingDefaultTheme();
}

SkColor PanelBrowserFrameView::GetTitleColor(PaintState paint_state) const {
  bool use_default = UsingDefaultTheme(paint_state);
  switch (paint_state) {
    case PAINT_AS_INACTIVE:
      return use_default ? kActiveTitleTextDefaultColor: SkColorSetA(
          GetThemeProvider()->GetColor(ThemeService::COLOR_BACKGROUND_TAB_TEXT),
          kInactiveAlphaBlending);
    case PAINT_AS_ACTIVE:
      return use_default ? kActiveTitleTextDefaultColor :
          GetThemeProvider()->GetColor(ThemeService::COLOR_TAB_TEXT);
    case PAINT_FOR_ATTENTION:
      return kAttentionTitleTextDefaultColor;
    default:
      NOTREACHED();
      return SkColor();
  }
}

SkBitmap* PanelBrowserFrameView::GetFrameTheme(PaintState paint_state) const {
  bool use_default = UsingDefaultTheme(paint_state);
  switch (paint_state) {
    case PAINT_AS_INACTIVE:
      return use_default ? GetInactiveBackgroundDefaultBitmap() :
          GetThemeProvider()->GetBitmapNamed(IDR_THEME_TAB_BACKGROUND);
    case PAINT_AS_ACTIVE:
      return use_default ? GetActiveBackgroundDefaultBitmap() :
          GetThemeProvider()->GetBitmapNamed(IDR_THEME_TOOLBAR);
    case PAINT_FOR_ATTENTION:
      return GetAttentionBackgroundDefaultBitmap();
    default:
      return GetInactiveBackgroundDefaultBitmap();
  }
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
  SkBitmap* bitmap = GetFrameTheme(paint_state_);

  // Top area, including title-bar.
  canvas->TileImageInt(*bitmap, 0, 0, width(), top_area_height);

  // For all the non-client area below titlebar, we paint it by using the last
  // line from the theme bitmap, instead of using the frame color provided
  // in the theme because the frame color in some themes is very different from
  // the tab colors we use to render the titlebar and it can produce abrupt
  // transition which looks bad.

  // Left border, below title-bar.
  canvas->DrawBitmapInt(*bitmap, 0, top_area_height - 1, thickness, 1,
      0, top_area_height, thickness, height() - top_area_height, false);

  // Right border, below title-bar.
  canvas->DrawBitmapInt(*bitmap, (width() % bitmap->width()) - thickness,
      top_area_height - 1, thickness, 1,
      width() - thickness, top_area_height,
      thickness, height() - top_area_height, false);

  // Bottom border.
  canvas->DrawBitmapInt(*bitmap, 0, top_area_height - 1, bitmap->width(), 1,
      0, height() - thickness, width(), thickness, false);
}

void PanelBrowserFrameView::PaintFrameEdge(gfx::Canvas* canvas) {
#if defined(USE_AURA)
  // Aura recognizes aura::client::WINDOW_TYPE_PANEL and will draw the
  // appropriate frame and shadow. See ash/wm/shadow_controller.h.

  // Draw the divider between the titlebar and the client area.
  if (!IsShowingTitlebarOnly()) {
    canvas->DrawRect(gfx::Rect(0, kTitlebarHeight, width() - 1, 1),
                     kDividerColor);
  }
#else
  // Draw the top border.
  const EdgeResources& frame_edges = GetFrameEdges();
  canvas->DrawBitmapInt(*(frame_edges.top_left), 0, 0);
  canvas->TileImageInt(
      *(frame_edges.top), frame_edges.top_left->width(), 0,
      width() - frame_edges.top_right->width(), frame_edges.top->height());
  canvas->DrawBitmapInt(
      *(frame_edges.top_right),
      width() - frame_edges.top_right->width(), 0);

  // Draw the right border.
  canvas->TileImageInt(
      *(frame_edges.right), width() - frame_edges.right->width(),
      frame_edges.top_right->height(), frame_edges.right->width(),
      height() - frame_edges.top_right->height() -
          frame_edges.bottom_right->height());

  // Draw the bottom border.
  canvas->DrawBitmapInt(
      *(frame_edges.bottom_right),
      width() - frame_edges.bottom_right->width(),
      height() - frame_edges.bottom_right->height());
  canvas->TileImageInt(
      *(frame_edges.bottom), frame_edges.bottom_left->width(),
      height() - frame_edges.bottom->height(),
      width() - frame_edges.bottom_left->width() -
          frame_edges.bottom_right->width(),
      frame_edges.bottom->height());
  canvas->DrawBitmapInt(
      *(frame_edges.bottom_left), 0,
      height() - frame_edges.bottom_left->height());

  // Draw the left border.
  canvas->TileImageInt(
      *(frame_edges.left), 0, frame_edges.top_left->height(),
      frame_edges.left->width(),
      height() - frame_edges.top_left->height() -
          frame_edges.bottom_left->height());

  // Draw the divider between the titlebar and the client area.
  if (!IsShowingTitlebarOnly() && !CanResize()) {
    canvas->DrawRect(gfx::Rect(NonClientBorderThickness(), kTitlebarHeight,
                               width() - 1 - 2 * NonClientBorderThickness(),
                               NonClientBorderThickness()), kDividerColor);
  }
#endif  // !defined(USE_AURA)
}

void PanelBrowserFrameView::PaintClientEdge(gfx::Canvas* canvas) {
#if !defined(USE_AURA)
  // No need to draw client edges when a non-resizable panel that has thin
  // border.
  if (!CanResize())
    return;

  gfx::Rect client_edge_bounds(client_view_bounds_);
  client_edge_bounds.Inset(-kResizableClientEdgeThickness,
                           -kResizableClientEdgeThickness);
  gfx::Rect frame_shadow_bounds(client_edge_bounds);
  frame_shadow_bounds.Inset(-kFrameShadowThickness, -kFrameShadowThickness);

  canvas->FillRect(frame_shadow_bounds, kContentsBorderShadow);
  canvas->FillRect(client_edge_bounds, kClientEdgeColor);
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
}

bool PanelBrowserFrameView::CanResize() const {
  return panel_browser_view_->panel()->CanResizeByMouse() !=
      panel::NOT_RESIZABLE;
}

bool PanelBrowserFrameView::IsShowingTitlebarOnly() const {
  return height() <= kTitlebarHeight;
}
