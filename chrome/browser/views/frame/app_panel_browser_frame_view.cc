// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/frame/app_panel_browser_frame_view.h"

#include "app/gfx/canvas.h"
#include "app/gfx/font.h"
#include "app/gfx/path.h"
#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/compiler_specific.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/views/frame/browser_frame.h"
#include "chrome/browser/views/frame/browser_view.h"
#include "grit/app_resources.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
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
// Besides the frame border, there's another 11 px of empty space atop the
// window in restored mode, to use to drag the window around.
const int kNonClientRestoredExtraThickness = 28;
// In the window corners, the resize areas don't actually expand bigger, but the
// 16 px at the end of each edge triggers diagonal resizing.
const int kResizeAreaCornerSize = 16;
// The titlebar never shrinks to less than 28 px tall, plus the height of the
// frame border and any bottom edge.
const int kTitlebarMinimumHeight = 28;
// The icon is inset 6 px from the left frame border.
const int kIconLeftSpacing = 6;
// The icon takes up 16/25th of the available titlebar height.  (This is
// expressed as two ints to avoid precision losses leading to off-by-one pixel
// errors.)
const int kIconHeightFractionNumerator = 16;
const int kIconHeightFractionDenominator = 25;
// The icon never shrinks below 16 px on a side.
const int kIconMinimumSize = 16;
// Because our frame border has a different "3D look" than Windows', with a less
// cluttered top edge, we need to shift the icon up by 1 px in restored mode so
// it looks more centered.
const int kIconRestoredAdjust = 1;
// There is a 4 px gap between the icon and the title text.
const int kIconTitleSpacing = 4;
// The title text starts 2 px below the bottom of the top frame border.
const int kTitleTopSpacing = 2;
// There is a 5 px gap between the title text and the close button.
const int kTitleCloseButtonSpacing = 5;
// To be centered the close button is offset by -4, -6 from the top, right
// corner.
const int kCloseButtonHorzSpacing = 4;
const int kCloseButtonVertSpacing = 6;
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
  close_button_->SetImage(
      views::CustomButton::BS_NORMAL,
      rb.GetBitmapNamed(IDR_CLOSE_BAR));
  close_button_->SetImage(
      views::CustomButton::BS_HOT,
      rb.GetBitmapNamed(IDR_CLOSE_BAR_H));
  close_button_->SetImage(
      views::CustomButton::BS_PUSHED,
      rb.GetBitmapNamed(IDR_CLOSE_BAR_P));
  close_button_->SetAccessibleName(l10n_util::GetString(IDS_ACCNAME_CLOSE));
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

void AppPanelBrowserFrameView::UpdateThrobber(bool running) {
  if (window_icon_)
    window_icon_->Update();
}

gfx::Size AppPanelBrowserFrameView::GetMinimumSize() {
  gfx::Size min_size(browser_view_->GetMinimumSize());
  int border_thickness = NonClientBorderThickness();
  min_size.Enlarge(2 * border_thickness,
                   NonClientTopBorderHeight() + border_thickness);

  int min_titlebar_width = (2 * FrameBorderThickness()) + kIconLeftSpacing +
      IconSize(NULL, NULL, NULL) + kTitleCloseButtonSpacing;

  min_size.set_width(std::max(min_size.width(), min_titlebar_width));

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
  if (frame_component != HTNOWHERE)
    return frame_component;

  // Then see if the point is within any of the window controls.
  if (close_button_->IsVisible() &&
      close_button_->GetBounds(APPLY_MIRRORING_TRANSFORMATION).Contains(point))
    return HTCLOSE;
  if (window_icon_ &&
      window_icon_->GetBounds(APPLY_MIRRORING_TRANSFORMATION).Contains(point))
    return HTSYSMENU;

  int window_component = GetHTComponentForFrame(point, FrameBorderThickness(),
      NonClientBorderThickness(), kResizeAreaCornerSize, kResizeAreaCornerSize,
      frame_->GetWindow()->GetDelegate()->CanResize());
  // Fall back to the caption if no other component matches.
  return (window_component == HTNOWHERE) ? HTCAPTION : window_component;
}

void AppPanelBrowserFrameView::GetWindowMask(const gfx::Size& size,
                                             gfx::Path* window_mask) {
  DCHECK(window_mask);
  DCHECK(!frame_->GetWindow()->IsMaximized() &&
         !frame_->GetWindow()->IsFullscreen());

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
  PaintRestoredFrameBorder(canvas);
  PaintTitleBar(canvas);
  PaintRestoredClientEdge(canvas);
}

void AppPanelBrowserFrameView::Layout() {
  LayoutWindowControls();
  LayoutTitleBar();
  LayoutClientView();
}

///////////////////////////////////////////////////////////////////////////////
// AppPanelBrowserFrameView, views::ButtonListener implementation:

void AppPanelBrowserFrameView::ButtonPressed(
    views::Button* sender, const views::Event& event) {
  views::Window* window = frame_->GetWindow();
  if (sender == close_button_)
    window->Close();
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
  return kFrameBorderThickness;
}

int AppPanelBrowserFrameView::NonClientBorderThickness() const {
  return FrameBorderThickness() + kClientEdgeThickness;
}

int AppPanelBrowserFrameView::NonClientTopBorderHeight() const {
  return TitleCoordinates(NULL, NULL);
}

int AppPanelBrowserFrameView::UnavailablePixelsAtBottomOfNonClientHeight() const
{
  return kFrameShadowThickness + kClientEdgeThickness;
}

int AppPanelBrowserFrameView::TitleCoordinates(int* title_top_spacing_ptr,
                                               int* title_thickness_ptr) const {
  int frame_thickness = FrameBorderThickness();
  int min_titlebar_height = kTitlebarMinimumHeight + frame_thickness;
  int title_top_spacing = frame_thickness + kTitleTopSpacing;
  // The bottom spacing should be the same apparent height as the top spacing.
  // Because the actual top spacing height varies based on the system border
  // thickness, we calculate this based on the restored top spacing and then
  // adjust for maximized mode.  We also don't include the frame shadow here,
  // since while it's part of the bottom spacing it will be added in at the end
  // as necessary (when a toolbar is present, the "shadow" is actually drawn by
  // the toolbar).
  int title_bottom_spacing =
      kFrameBorderThickness + kTitleTopSpacing - kFrameShadowThickness;

  int title_thickness = std::max(BrowserFrame::GetTitleFont().height(),
      min_titlebar_height - title_top_spacing - title_bottom_spacing);
  if (title_top_spacing_ptr)
    *title_top_spacing_ptr = title_top_spacing;
  if (title_thickness_ptr)
    *title_thickness_ptr = title_thickness;
  return title_top_spacing + title_thickness + title_bottom_spacing +
      UnavailablePixelsAtBottomOfNonClientHeight();
}

int AppPanelBrowserFrameView::RightEdge() const {
  return width() - FrameBorderThickness();
}

int AppPanelBrowserFrameView::IconSize(int* title_top_spacing_ptr,
                                       int* title_thickness_ptr,
                                       int* available_height_ptr) const {
  // The usable height of the titlebar area is the total height minus the top
  // resize border and any edge area we draw at its bottom.
  int frame_thickness = FrameBorderThickness();
  int top_height = TitleCoordinates(title_top_spacing_ptr, title_thickness_ptr);
  int available_height = top_height - frame_thickness -
      UnavailablePixelsAtBottomOfNonClientHeight();
  if (available_height_ptr)
    *available_height_ptr = available_height;

  // The icon takes up a constant fraction of the available height, down to a
  // minimum size, and is always an even number of pixels on a side (presumably
  // to make scaled icons look better).  It's centered within the usable height.
  return std::max((available_height * kIconHeightFractionNumerator /
      kIconHeightFractionDenominator) / 2 * 2, kIconMinimumSize);
}

void AppPanelBrowserFrameView::PaintRestoredFrameBorder(gfx::Canvas* canvas) {
  // Window frame mode.
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();

  SkBitmap* frame_image;
  SkColor frame_color;

  if (ShouldPaintAsActive()) {
    frame_image = rb.GetBitmapNamed(IDR_FRAME_APP_PANEL);
    frame_color = ResourceBundle::frame_color_app_panel;
  } else {
    // TODO: Differentiate inactive
    frame_image = rb.GetBitmapNamed(IDR_FRAME_APP_PANEL);
    frame_color = ResourceBundle::frame_color_app_panel;
  }

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

  // Fill with the frame color first so we have a constant background for
  // areas not covered by the theme image.
  canvas->FillRectInt(frame_color, 0, 0, width(), frame_image->height());
  // Now fill down the sides
  canvas->FillRectInt(frame_color,
      0, frame_image->height(),
      left_edge->width(), height() - frame_image->height());
  canvas->FillRectInt(frame_color,
      width() - right_edge->width(), frame_image->height(),
      right_edge->width(), height() - frame_image->height());
  // Now fill the bottom area.
  canvas->FillRectInt(frame_color,
      left_edge->width(), height() - bottom_edge->height(),
      width() - left_edge->width() - right_edge->width(),
      bottom_edge->height());

  // Draw the theme frame.
  canvas->TileImageInt(*frame_image, 0, 0, width(), frame_image->height());

  // Top.
  canvas->DrawBitmapInt(*top_left_corner, 0, 0);
  canvas->TileImageInt(*top_edge, top_left_corner->width(), 0,
                       width() - top_right_corner->width(), top_edge->height());
  canvas->DrawBitmapInt(*top_right_corner,
                        width() - top_right_corner->width(), 0);

  // Right.
  canvas->TileImageInt(*right_edge, width() - right_edge->width(),
                       top_right_corner->height(), right_edge->width(),
                       height() - top_right_corner->height() -
                           bottom_right_corner->height());

  // Bottom.
  canvas->DrawBitmapInt(*bottom_right_corner,
                        width() - bottom_right_corner->width(),
                        height() - bottom_right_corner->height());
  canvas->TileImageInt(*bottom_edge, bottom_left_corner->width(),
                       height() - bottom_edge->height(),
                       width() - bottom_left_corner->width() -
                           bottom_right_corner->width(),
                       bottom_edge->height());
  canvas->DrawBitmapInt(*bottom_left_corner, 0,
                        height() - bottom_left_corner->height());

  // Left.
  canvas->TileImageInt(*left_edge, 0, top_left_corner->height(),
      left_edge->width(),
      height() - top_left_corner->height() - bottom_left_corner->height());
}

void AppPanelBrowserFrameView::PaintTitleBar(gfx::Canvas* canvas) {
  // The window icon is painted by the TabIconView.
  views::WindowDelegate* d = frame_->GetWindow()->GetDelegate();
  canvas->DrawStringInt(d->GetWindowTitle(), BrowserFrame::GetTitleFont(),
      SK_ColorBLACK, MirroredLeftPointForRect(title_bounds_), title_bounds_.y(),
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
      client_area_bounds.x() - 1, client_area_top - 1,
      client_area_bounds.width() + 1, client_area_bottom - client_area_top + 1);
}

void AppPanelBrowserFrameView::LayoutWindowControls() {
  close_button_->SetImageAlignment(views::ImageButton::ALIGN_LEFT,
                                   views::ImageButton::ALIGN_BOTTOM);
  // Maximized buttons start at window top so that even if their images aren't
  // drawn flush with the screen edge, they still obey Fitts' Law.
  int caption_y =  kFrameShadowThickness;
  // There should always be the same number of non-shadow pixels visible to the
  // side of the caption buttons.  In maximized mode we extend the rightmost
  // button to the screen corner to obey Fitts' Law.
  int right_extra_width = kCloseButtonHorzSpacing;
  gfx::Size close_button_size = close_button_->GetPreferredSize();
  close_button_->SetBounds(
      RightEdge() - close_button_size.width() - right_extra_width,
      caption_y + kCloseButtonVertSpacing,
      close_button_size.width() + right_extra_width,
      close_button_size.height());
}

void AppPanelBrowserFrameView::LayoutTitleBar() {
  // Always lay out the icon, even when it's not present, so we can lay out the
  // window title based on its position.
  int frame_thickness = FrameBorderThickness();
  int icon_x = frame_thickness + kIconLeftSpacing;
  int title_top_spacing, title_thickness, available_height;
  int icon_size = 0;

  icon_size =
      IconSize(&title_top_spacing, &title_thickness, &available_height);
  int icon_y = ((available_height - icon_size) / 2) + frame_thickness;

  // Hack: Our frame border has a different "3D look" than Windows'.
  // Theirs has a more complex gradient on the top that they push
  // their icon/title below; then the maximized window cuts this off
  // and the icon/title are centered in the remaining space.
  // Because the apparent shape of our border is simpler, using the
  // same positioning makes things look slightly uncentered with
  // restored windows, so we come up to compensate.
  icon_y -= kIconRestoredAdjust;

  window_icon_->SetBounds(icon_x, icon_y, icon_size, icon_size);

  int title_font_height = BrowserFrame::GetTitleFont().height();
  int title_x = icon_x + icon_size + kIconTitleSpacing;
  title_bounds_.SetRect(title_x,
      title_top_spacing + ((title_thickness - title_font_height) / 2),
      std::max(0, close_button_->x() - kTitleCloseButtonSpacing - title_x),
      title_font_height);
}

void AppPanelBrowserFrameView::LayoutClientView() {
  client_view_bounds_ = CalculateClientAreaBounds(width(), height());
}

gfx::Rect AppPanelBrowserFrameView::CalculateClientAreaBounds(int width,
    int height) const {
  int top_height = NonClientTopBorderHeight();
  int border_thickness = NonClientBorderThickness();
  return gfx::Rect(border_thickness, top_height,
                   std::max(0, width - (2 * border_thickness)),
                   std::max(0, height - top_height - border_thickness));
}
