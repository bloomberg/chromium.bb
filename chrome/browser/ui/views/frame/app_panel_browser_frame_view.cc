// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/app_panel_browser_frame_view.h"

#include "base/compiler_specific.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/ui/views/frame/browser_frame.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "grit/app_resources.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font.h"
#include "ui/gfx/path.h"
#include "views/controls/button/image_button.h"
#include "views/window/window.h"
#include "views/window/window_resources.h"

#if defined(OS_LINUX)
#include "views/window/hit_test.h"
#endif

namespace {
// The frame border is only visible in restored mode and is hardcoded to 1 px on
// each side regardless of the system window border size.
const int kFrameBorderThickness = 1;
// In the window corners, the resize areas don't actually expand bigger, but the
// 16 px at the end of each edge triggers diagonal resizing.
const int kResizeAreaCornerSize = 16;
// The titlebar never shrinks too short to show the caption button plus some
// padding below it.
const int kCaptionButtonHeightWithPadding = 27;
// The titlebar has a 2 px 3D edge along the bottom, and we reserve 2 px (1 for
// border, 1 for padding) along the top.
const int kTitlebarTopAndBottomEdgeThickness = 2;
// The icon is inset 6 px from the left frame border.
const int kIconLeftSpacing = 6;
// The icon never shrinks below 16 px on a side.
const int kIconMinimumSize = 16;
// There is a 4 px gap between the icon and the title text.
const int kIconTitleSpacing = 4;
// There is a 5 px gap between the title text and the close button.
const int kTitleCloseButtonSpacing = 5;
// There is a 4 px gap between the close button and the frame border.
const int kCloseButtonFrameBorderSpacing = 4;
}

///////////////////////////////////////////////////////////////////////////////
// AppPanelBrowserFrameView, public:

AppPanelBrowserFrameView::AppPanelBrowserFrameView(BrowserFrame* frame,
                                                   BrowserView* browser_view)
    : BrowserNonClientFrameView(),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          close_button_(new views::ImageButton(this))),
      window_icon_(NULL),
      frame_(frame),
      browser_view_(browser_view) {
  DCHECK(browser_view->ShouldShowWindowIcon());
  DCHECK(browser_view->ShouldShowWindowTitle());

  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  close_button_->SetImage(views::CustomButton::BS_NORMAL,
                          rb.GetBitmapNamed(IDR_CLOSE_BAR));
  close_button_->SetImage(views::CustomButton::BS_HOT,
                          rb.GetBitmapNamed(IDR_CLOSE_BAR_H));
  close_button_->SetImage(views::CustomButton::BS_PUSHED,
                          rb.GetBitmapNamed(IDR_CLOSE_BAR_P));
  close_button_->SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_ACCNAME_CLOSE));
  AddChildView(close_button_);

  window_icon_ = new TabIconView(this);
  window_icon_->set_is_light(true);
  AddChildView(window_icon_);
  window_icon_->Update();
}

AppPanelBrowserFrameView::~AppPanelBrowserFrameView() {
}

///////////////////////////////////////////////////////////////////////////////
// AppPanelBrowserFrameView, BrowserNonClientFrameView implementation:

gfx::Rect AppPanelBrowserFrameView::GetBoundsForTabStrip(
    BaseTabStrip* tabstrip) const {
  // App panels never show a tab strip.
  NOTREACHED();
  return gfx::Rect();
}

int AppPanelBrowserFrameView::GetHorizontalTabStripVerticalOffset(
    bool restored) const {
  // App panels are not themed and don't need this.
  return 0;
}

void AppPanelBrowserFrameView::UpdateThrobber(bool running) {
  window_icon_->Update();
}

gfx::Size AppPanelBrowserFrameView::GetMinimumSize() {
  gfx::Size min_size(browser_view_->GetMinimumSize());
  int border_thickness = NonClientBorderThickness();
  min_size.Enlarge(2 * border_thickness,
                   NonClientTopBorderHeight() + border_thickness);

  min_size.set_width(std::max(min_size.width(),
      (2 * FrameBorderThickness()) + kIconLeftSpacing + IconSize() +
      kTitleCloseButtonSpacing + kCloseButtonFrameBorderSpacing));
  return min_size;
}

///////////////////////////////////////////////////////////////////////////////
// AppPanelBrowserFrameView, views::NonClientFrameView implementation:

gfx::Rect AppPanelBrowserFrameView::GetBoundsForClientView() const {
  return client_view_bounds_;
}

bool AppPanelBrowserFrameView::AlwaysUseCustomFrame() const {
  return true;
}

bool AppPanelBrowserFrameView::AlwaysUseNativeFrame() const {
  return frame_->AlwaysUseNativeFrame();
}

gfx::Rect AppPanelBrowserFrameView::GetWindowBoundsForClientBounds(
    const gfx::Rect& client_bounds) const {
  int top_height = NonClientTopBorderHeight();
  int border_thickness = NonClientBorderThickness();
  return gfx::Rect(std::max(0, client_bounds.x() - border_thickness),
                   std::max(0, client_bounds.y() - top_height),
                   client_bounds.width() + (2 * border_thickness),
                   client_bounds.height() + top_height + border_thickness);
}

int AppPanelBrowserFrameView::NonClientHitTest(const gfx::Point& point) {
  if (!bounds().Contains(point))
    return HTNOWHERE;

  int frame_component =
      frame_->GetWindow()->GetClientView()->NonClientHitTest(point);

  // See if we're in the sysmenu region.  (We check the ClientView first to be
  // consistent with OpaqueBrowserFrameView; it's not really necessary here.)
  gfx::Rect sysmenu_rect(IconBounds());
  // In maximized mode we extend the rect to the screen corner to take advantage
  // of Fitts' Law.
  if (frame_->GetWindow()->IsMaximized())
    sysmenu_rect.SetRect(0, 0, sysmenu_rect.right(), sysmenu_rect.bottom());
  sysmenu_rect.set_x(GetMirroredXForRect(sysmenu_rect));
  if (sysmenu_rect.Contains(point))
    return (frame_component == HTCLIENT) ? HTCLIENT : HTSYSMENU;

  if (frame_component != HTNOWHERE)
    return frame_component;

  // Then see if the point is within any of the window controls.
  if (close_button_->IsVisible() &&
      close_button_->GetMirroredBounds().Contains(point))
    return HTCLOSE;

  int window_component = GetHTComponentForFrame(point,
      NonClientBorderThickness(), NonClientBorderThickness(),
      kResizeAreaCornerSize, kResizeAreaCornerSize,
      frame_->GetWindow()->GetDelegate()->CanResize());
  // Fall back to the caption if no other component matches.
  return (window_component == HTNOWHERE) ? HTCAPTION : window_component;
}

void AppPanelBrowserFrameView::GetWindowMask(const gfx::Size& size,
                                             gfx::Path* window_mask) {
  DCHECK(window_mask);

  if (frame_->GetWindow()->IsMaximized())
    return;

  // Redefine the window visible region for the new size.
  window_mask->moveTo(0, 3);
  window_mask->lineTo(1, 2);
  window_mask->lineTo(1, 1);
  window_mask->lineTo(2, 1);
  window_mask->lineTo(3, 0);

  window_mask->lineTo(SkIntToScalar(size.width() - 3), 0);
  window_mask->lineTo(SkIntToScalar(size.width() - 2), 1);
  window_mask->lineTo(SkIntToScalar(size.width() - 1), 1);
  window_mask->lineTo(SkIntToScalar(size.width() - 1), 2);
  window_mask->lineTo(SkIntToScalar(size.width()), 3);

  window_mask->lineTo(SkIntToScalar(size.width()),
                      SkIntToScalar(size.height()));
  window_mask->lineTo(0, SkIntToScalar(size.height()));
  window_mask->close();
}

void AppPanelBrowserFrameView::EnableClose(bool enable) {
  close_button_->SetEnabled(enable);
}

void AppPanelBrowserFrameView::ResetWindowControls() {
  // The close button isn't affected by this constraint.
}

///////////////////////////////////////////////////////////////////////////////
// AppPanelBrowserFrameView, views::View overrides:

void AppPanelBrowserFrameView::Paint(gfx::Canvas* canvas) {
  views::Window* window = frame_->GetWindow();
  if (window->IsMaximized())
    PaintMaximizedFrameBorder(canvas);
  else
    PaintRestoredFrameBorder(canvas);
  PaintTitleBar(canvas);
  if (!window->IsMaximized())
    PaintRestoredClientEdge(canvas);
}

void AppPanelBrowserFrameView::Layout() {
  LayoutWindowControls();
  LayoutTitleBar();
  client_view_bounds_ = CalculateClientAreaBounds(width(), height());
}

///////////////////////////////////////////////////////////////////////////////
// AppPanelBrowserFrameView, views::ButtonListener implementation:

void AppPanelBrowserFrameView::ButtonPressed(views::Button* sender,
                                             const views::Event& event) {
  if (sender == close_button_)
    frame_->GetWindow()->Close();
}

///////////////////////////////////////////////////////////////////////////////
// AppPanelBrowserFrameView, TabIconView::TabContentsProvider implementation:

bool AppPanelBrowserFrameView::ShouldTabIconViewAnimate() const {
  // This function is queried during the creation of the window as the
  // TabIconView we host is initialized, so we need to NULL check the selected
  // TabContents because in this condition there is not yet a selected tab.
  TabContents* current_tab = browser_view_->GetSelectedTabContents();
  return current_tab ? current_tab->is_loading() : false;
}

SkBitmap AppPanelBrowserFrameView::GetFavIconForTabIconView() {
  return frame_->GetWindow()->GetDelegate()->GetWindowIcon();
}

///////////////////////////////////////////////////////////////////////////////
// AppPanelBrowserFrameView, private:

int AppPanelBrowserFrameView::FrameBorderThickness() const {
  return frame_->GetWindow()->IsMaximized() ? 0 : kFrameBorderThickness;
}

int AppPanelBrowserFrameView::NonClientBorderThickness() const {
  return FrameBorderThickness() +
      (frame_->GetWindow()->IsMaximized() ? 0 : kClientEdgeThickness);
}

int AppPanelBrowserFrameView::NonClientTopBorderHeight() const {
  return std::max(FrameBorderThickness() + IconSize(),
                  FrameBorderThickness() + kCaptionButtonHeightWithPadding) +
      TitlebarBottomThickness();
}

int AppPanelBrowserFrameView::TitlebarBottomThickness() const {
  return kTitlebarTopAndBottomEdgeThickness +
      (frame_->GetWindow()->IsMaximized() ? 0 : kClientEdgeThickness);
}

int AppPanelBrowserFrameView::IconSize() const {
#if defined(OS_WIN)
  // This metric scales up if either the titlebar height or the titlebar font
  // size are increased.
  return GetSystemMetrics(SM_CYSMICON);
#else
  return std::max(BrowserFrame::GetTitleFont().height(), kIconMinimumSize);
#endif
}

gfx::Rect AppPanelBrowserFrameView::IconBounds() const {
  int size = IconSize();
  int frame_thickness = FrameBorderThickness();
  // Our frame border has a different "3D look" than Windows'.  Theirs has a
  // more complex gradient on the top that they push their icon/title below;
  // then the maximized window cuts this off and the icon/title are centered
  // in the remaining space.  Because the apparent shape of our border is
  // simpler, using the same positioning makes things look slightly uncentered
  // with restored windows, so when the window is restored, instead of
  // calculating the remaining space from below the frame border, we calculate
  // from below the top border-plus-padding.
  int unavailable_px_at_top = frame_->GetWindow()->IsMaximized() ?
      frame_thickness : kTitlebarTopAndBottomEdgeThickness;
  // When the icon is shorter than the minimum space we reserve for the caption
  // button, we vertically center it.  We want to bias rounding to put extra
  // space above the icon, since the 3D edge (+ client edge, for restored
  // windows) below looks (to the eye) more like additional space than does the
  // border + padding (or nothing at all, for maximized windows) above; hence
  // the +1.
  int y = unavailable_px_at_top + (NonClientTopBorderHeight() -
      unavailable_px_at_top - size - TitlebarBottomThickness() + 1) / 2;
  return gfx::Rect(frame_thickness + kIconLeftSpacing, y, size, size);
}

void AppPanelBrowserFrameView::PaintRestoredFrameBorder(gfx::Canvas* canvas) {
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();

  SkBitmap* top_left_corner = rb.GetBitmapNamed(IDR_WINDOW_TOP_LEFT_CORNER);
  SkBitmap* top_right_corner =
      rb.GetBitmapNamed(IDR_WINDOW_TOP_RIGHT_CORNER);
  SkBitmap* top_edge = rb.GetBitmapNamed(IDR_WINDOW_TOP_CENTER);
  SkBitmap* right_edge = rb.GetBitmapNamed(IDR_WINDOW_RIGHT_SIDE);
  SkBitmap* left_edge = rb.GetBitmapNamed(IDR_WINDOW_LEFT_SIDE);
  SkBitmap* bottom_left_corner =
      rb.GetBitmapNamed(IDR_WINDOW_BOTTOM_LEFT_CORNER);
  SkBitmap* bottom_right_corner =
      rb.GetBitmapNamed(IDR_WINDOW_BOTTOM_RIGHT_CORNER);
  SkBitmap* bottom_edge = rb.GetBitmapNamed(IDR_WINDOW_BOTTOM_CENTER);

  // Window frame mode and color.
  SkBitmap* theme_frame;
  SkColor frame_color;
  if (ShouldPaintAsActive()) {
    theme_frame = rb.GetBitmapNamed(IDR_FRAME_APP_PANEL);
    frame_color = ResourceBundle::frame_color_app_panel;
  } else {
    theme_frame = rb.GetBitmapNamed(IDR_FRAME_APP_PANEL);  // TODO
    frame_color = ResourceBundle::frame_color_app_panel_inactive;
  }

  // Fill with the frame color first so we have a constant background for
  // areas not covered by the theme image.
  canvas->FillRectInt(frame_color, 0, 0, width(), theme_frame->height());
  // Now fill down the sides.
  canvas->FillRectInt(frame_color, 0, theme_frame->height(), left_edge->width(),
                      height() - theme_frame->height());
  canvas->FillRectInt(frame_color, width() - right_edge->width(),
                      theme_frame->height(), right_edge->width(),
                      height() - theme_frame->height());
  // Now fill the bottom area.
  canvas->FillRectInt(frame_color, left_edge->width(),
                      height() - bottom_edge->height(),
                      width() - left_edge->width() - right_edge->width(),
                      bottom_edge->height());

  // Draw the theme frame.
  canvas->TileImageInt(*theme_frame, 0, 0, width(), theme_frame->height());

  // Top.
  canvas->DrawBitmapInt(*top_left_corner, 0, 0);
  canvas->TileImageInt(*top_edge, top_left_corner->width(), 0,
                       width() - top_right_corner->width(), top_edge->height());
  canvas->DrawBitmapInt(*top_right_corner,
                        width() - top_right_corner->width(), 0);

  // Right.
  canvas->TileImageInt(*right_edge, width() - right_edge->width(),
      top_right_corner->height(), right_edge->width(),
      height() - top_right_corner->height() - bottom_right_corner->height());

  // Bottom.
  canvas->DrawBitmapInt(*bottom_right_corner,
                        width() - bottom_right_corner->width(),
                        height() - bottom_right_corner->height());
  canvas->TileImageInt(*bottom_edge, bottom_left_corner->width(),
      height() - bottom_edge->height(),
      width() - bottom_left_corner->width() - bottom_right_corner->width(),
      bottom_edge->height());
  canvas->DrawBitmapInt(*bottom_left_corner, 0,
                        height() - bottom_left_corner->height());

  // Left.
  canvas->TileImageInt(*left_edge, 0, top_left_corner->height(),
      left_edge->width(),
      height() - top_left_corner->height() - bottom_left_corner->height());
}

void AppPanelBrowserFrameView::PaintMaximizedFrameBorder(gfx::Canvas* canvas) {
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();

  SkBitmap* frame_image = rb.GetBitmapNamed(IDR_FRAME_APP_PANEL);
  canvas->TileImageInt(*frame_image, 0, FrameBorderThickness(), width(),
                       frame_image->height());

  // The bottom of the titlebar actually comes from the top of the Client Edge
  // graphic, with the actual client edge clipped off the bottom.
  SkBitmap* titlebar_bottom = rb.GetBitmapNamed(IDR_APP_TOP_CENTER);
  int edge_height = titlebar_bottom->height() - kClientEdgeThickness;
  canvas->TileImageInt(*titlebar_bottom, 0,
                       frame_->GetWindow()->GetClientView()->y() - edge_height,
                       width(), edge_height);
}

void AppPanelBrowserFrameView::PaintTitleBar(gfx::Canvas* canvas) {
  // The window icon is painted by the TabIconView.
  views::WindowDelegate* d = frame_->GetWindow()->GetDelegate();
  canvas->DrawStringInt(d->GetWindowTitle(), BrowserFrame::GetTitleFont(),
      SK_ColorBLACK, GetMirroredXForRect(title_bounds_), title_bounds_.y(),
      title_bounds_.width(), title_bounds_.height());
}

void AppPanelBrowserFrameView::PaintRestoredClientEdge(gfx::Canvas* canvas) {
  gfx::Rect client_area_bounds = CalculateClientAreaBounds(width(), height());
  int client_area_top = client_area_bounds.y();

  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  SkBitmap* top_left = rb.GetBitmapNamed(IDR_APP_TOP_LEFT);
  SkBitmap* top = rb.GetBitmapNamed(IDR_APP_TOP_CENTER);
  SkBitmap* top_right = rb.GetBitmapNamed(IDR_APP_TOP_RIGHT);
  SkBitmap* right = rb.GetBitmapNamed(IDR_CONTENT_RIGHT_SIDE);
  SkBitmap* bottom_right =
      rb.GetBitmapNamed(IDR_CONTENT_BOTTOM_RIGHT_CORNER);
  SkBitmap* bottom = rb.GetBitmapNamed(IDR_CONTENT_BOTTOM_CENTER);
  SkBitmap* bottom_left =
      rb.GetBitmapNamed(IDR_CONTENT_BOTTOM_LEFT_CORNER);
  SkBitmap* left = rb.GetBitmapNamed(IDR_CONTENT_LEFT_SIDE);

  // Top.
  int top_edge_y = client_area_top - top->height();
  canvas->DrawBitmapInt(*top_left, client_area_bounds.x() - top_left->width(),
                        top_edge_y);
  canvas->TileImageInt(*top, client_area_bounds.x(), top_edge_y,
                       client_area_bounds.width(), top->height());
  canvas->DrawBitmapInt(*top_right, client_area_bounds.right(), top_edge_y);

  // Right.
  int client_area_bottom =
      std::max(client_area_top, client_area_bounds.bottom());
  int client_area_height = client_area_bottom - client_area_top;
  canvas->TileImageInt(*right, client_area_bounds.right(), client_area_top,
                       right->width(), client_area_height);

  // Bottom.
  canvas->DrawBitmapInt(*bottom_right, client_area_bounds.right(),
                        client_area_bottom);
  canvas->TileImageInt(*bottom, client_area_bounds.x(), client_area_bottom,
                       client_area_bounds.width(), bottom_right->height());
  canvas->DrawBitmapInt(*bottom_left,
      client_area_bounds.x() - bottom_left->width(), client_area_bottom);

  // Left.
  canvas->TileImageInt(*left, client_area_bounds.x() - left->width(),
      client_area_top, left->width(), client_area_height);

  // Draw the toolbar color to fill in the edges.
  canvas->DrawRectInt(ResourceBundle::toolbar_color,
      client_area_bounds.x() - kClientEdgeThickness,
      client_area_top - kClientEdgeThickness,
      client_area_bounds.width() + kClientEdgeThickness,
      client_area_bottom - client_area_top + kClientEdgeThickness);
}

void AppPanelBrowserFrameView::LayoutWindowControls() {
  close_button_->SetImageAlignment(views::ImageButton::ALIGN_LEFT,
                                   views::ImageButton::ALIGN_BOTTOM);
  bool is_maximized = frame_->GetWindow()->IsMaximized();
  // There should always be the same number of non-border pixels visible to the
  // side of the close button.  In maximized mode we extend the button to the
  // screen corner to obey Fitts' Law.
  int right_extra_width = is_maximized ? kCloseButtonFrameBorderSpacing : 0;
  gfx::Size close_button_size = close_button_->GetPreferredSize();
  int close_button_y =
      (NonClientTopBorderHeight() - close_button_size.height()) / 2;
  int top_extra_height = is_maximized ? close_button_y : 0;
  close_button_->SetBounds(width() - FrameBorderThickness() -
      kCloseButtonFrameBorderSpacing - close_button_size.width(),
      close_button_y - top_extra_height,
      close_button_size.width() + right_extra_width,
      close_button_size.height() + top_extra_height);
}

void AppPanelBrowserFrameView::LayoutTitleBar() {
  // Size the icon first; the window title is based on the icon position.
  gfx::Rect icon_bounds(IconBounds());
  window_icon_->SetBoundsRect(icon_bounds);

  // Size the title.
  int title_x = icon_bounds.right() + kIconTitleSpacing;
  int title_height = BrowserFrame::GetTitleFont().GetHeight();
  // We bias the title position so that when the difference between the icon
  // and title heights is odd, the extra pixel of the title is above the
  // vertical midline rather than below.  This compensates for how the icon is
  // already biased downwards (see IconBounds()) and helps prevent descenders
  // on the title from overlapping the 3D edge at the bottom of the titlebar.
  title_bounds_.SetRect(title_x,
      icon_bounds.y() + ((icon_bounds.height() - title_height - 1) / 2),
      std::max(0, close_button_->x() - kTitleCloseButtonSpacing - title_x),
      title_height);
}

gfx::Rect AppPanelBrowserFrameView::CalculateClientAreaBounds(int width,
    int height) const {
  int top_height = NonClientTopBorderHeight();
  int border_thickness = NonClientBorderThickness();
  return gfx::Rect(border_thickness, top_height,
                   std::max(0, width - (2 * border_thickness)),
                   std::max(0, height - top_height - border_thickness));
}
