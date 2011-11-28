// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/panels/panel_browser_frame_view.h"

#include <algorithm>

#include "base/utf_string_conversions.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/ui/panels/panel.h"
#include "chrome/browser/ui/panels/panel_browser_view.h"
#include "chrome/browser/ui/panels/panel_manager.h"
#include "chrome/browser/ui/panels/panel_settings_menu_model.h"
#include "chrome/common/extensions/extension.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "grit/theme_resources_standard.h"
#include "grit/ui_resources.h"
#include "third_party/skia/include/effects/SkGradientShader.h"
#include "ui/base/accessibility/accessible_view_state.h"
#include "ui/base/animation/slide_animation.h"
#include "ui/base/hit_test.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas_skia.h"
#include "ui/gfx/path.h"
#include "ui/gfx/screen.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/menu_model_adapter.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/widget/widget_delegate.h"
#include "views/controls/label.h"
#include "views/painter.h"

namespace {

// The height in pixels of the titlebar.
const int kTitlebarHeight = 24;

// The thickness in pixels of the frame border.
const int kFrameBorderThickness = 1;

// The spacing in pixels between the icon and the border/text.
const int kIconSpacing = 4;

// The height and width in pixels of the icon.
const int kIconSize = 16;

// The spacing in pixels between buttons or the button and the adjacent control.
const int kButtonSpacing = 6;

// This value is experimental and subjective.
const int kUpdateSettingsVisibilityAnimationMs = 120;

// Colors used in painting the titlebar for drawing attention.
const SkColor kBackgroundColorForAttention = 0xfffa983a;
const SkColor kTitleTextColorForAttention = SK_ColorWHITE;

struct ButtonResources {
  SkBitmap* normal_image;
  SkBitmap* mask_image;
  SkBitmap* hover_image;
  SkBitmap* pushed_image;

  void SetResources(int normal_image_id, int mask_image_id, int hover_image_id,
                    int pushed_image_id) {
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    normal_image = rb.GetBitmapNamed(normal_image_id);
    mask_image = mask_image_id ? rb.GetBitmapNamed(mask_image_id) : NULL;
    hover_image = rb.GetBitmapNamed(hover_image_id);
    pushed_image = rb.GetBitmapNamed(pushed_image_id);
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

  void SetResources(int top_left_id, int top_id, int top_right_id, int right_id,
                    int bottom_right_id, int bottom_id, int bottom_left_id,
                    int left_id) {
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

ButtonResources settings_button_resources;
ButtonResources close_button_resources;
EdgeResources frame_edges;
EdgeResources client_edges;
gfx::Font* title_font = NULL;
SkBitmap* background_bitmap_for_attention = NULL;

void LoadImageResources() {
  settings_button_resources.SetResources(
      IDR_BALLOON_WRENCH, 0, IDR_BALLOON_WRENCH_H, IDR_BALLOON_WRENCH_P);

  close_button_resources.SetResources(
      IDR_TAB_CLOSE, IDR_TAB_CLOSE_MASK, IDR_TAB_CLOSE_H, IDR_TAB_CLOSE_P);

  frame_edges.SetResources(
      IDR_WINDOW_TOP_LEFT_CORNER, IDR_WINDOW_TOP_CENTER,
      IDR_WINDOW_TOP_RIGHT_CORNER, IDR_WINDOW_RIGHT_SIDE,
      IDR_PANEL_BOTTOM_RIGHT_CORNER, IDR_WINDOW_BOTTOM_CENTER,
      IDR_PANEL_BOTTOM_LEFT_CORNER, IDR_WINDOW_LEFT_SIDE);

  client_edges.SetResources(
      IDR_APP_TOP_LEFT, IDR_APP_TOP_CENTER,
      IDR_APP_TOP_RIGHT, IDR_CONTENT_RIGHT_SIDE,
      IDR_CONTENT_BOTTOM_RIGHT_CORNER, IDR_CONTENT_BOTTOM_CENTER,
      IDR_CONTENT_BOTTOM_LEFT_CORNER, IDR_CONTENT_LEFT_SIDE);
}

void EnsureResourcesInitialized() {
  static bool resources_initialized = false;
  if (resources_initialized)
    return;
  resources_initialized = true;

  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  title_font = new gfx::Font(rb.GetFont(ResourceBundle::BaseFont));

  // Creates a bitmap of the specified color.
  background_bitmap_for_attention = new SkBitmap();
  background_bitmap_for_attention->setConfig(
      SkBitmap::kARGB_8888_Config, 16, 16);
  background_bitmap_for_attention->allocPixels();
  background_bitmap_for_attention->eraseColor(kBackgroundColorForAttention);

  LoadImageResources();
}

}  // namespace

// PanelBrowserFrameView::MouseWatcher -----------------------------------------

PanelBrowserFrameView::MouseWatcher::MouseWatcher(PanelBrowserFrameView* view)
    : view_(view),
      is_mouse_within_(false) {
  MessageLoopForUI::current()->AddObserver(this);
}

PanelBrowserFrameView::MouseWatcher::~MouseWatcher() {
  MessageLoopForUI::current()->RemoveObserver(this);
}

bool PanelBrowserFrameView::MouseWatcher::IsCursorInViewBounds() const {
  gfx::Point cursor_point = gfx::Screen::GetCursorScreenPoint();
  return view_->browser_view()->GetBounds().Contains(cursor_point.x(),
                                                     cursor_point.y());
}

#if defined(OS_WIN)
base::EventStatus PanelBrowserFrameView::MouseWatcher::WillProcessEvent(
    const base::NativeEvent& event) {
  return base::EVENT_CONTINUE;
}

void PanelBrowserFrameView::MouseWatcher::DidProcessEvent(
    const base::NativeEvent& event) {
  switch (event.message) {
    case WM_MOUSEMOVE:
    case WM_NCMOUSEMOVE:
    case WM_MOUSELEAVE:
    case WM_NCMOUSELEAVE:
      HandleGlobalMouseMoveEvent();
      break;
    default:
      break;
  }
}
#elif defined(USE_AURA)
base::EventStatus PanelBrowserFrameView::MouseWatcher::WillProcessEvent(
    const base::NativeEvent& event) {
  return base::EVENT_CONTINUE;
}

void PanelBrowserFrameView::MouseWatcher::DidProcessEvent(
    const base::NativeEvent& event) {
  NOTIMPLEMENTED();
}
#elif defined(TOOLKIT_USES_GTK)
void PanelBrowserFrameView::MouseWatcher::WillProcessEvent(GdkEvent* event) {
}

void PanelBrowserFrameView::MouseWatcher::DidProcessEvent(GdkEvent* event) {
  switch (event->type) {
    case GDK_MOTION_NOTIFY:
    case GDK_LEAVE_NOTIFY:
      HandleGlobalMouseMoveEvent();
      break;
    default:
      break;
  }
}
#elif defined(USE_AURA)
base::MessagePumpObserver::EventStatus
PanelBrowserFrameView::MouseWatcher::WillProcessXEvent(
    XEvent* xevent) {
  NOTIMPLEMENTED();
  return EVENT_CONTINUE;
}
#endif

void PanelBrowserFrameView::MouseWatcher::HandleGlobalMouseMoveEvent() {
  bool is_mouse_within = IsCursorInViewBounds();
  if (is_mouse_within == is_mouse_within_)
    return;
  is_mouse_within_ = is_mouse_within;
  view_->OnMouseEnterOrLeaveWindow(is_mouse_within_);
}

// PanelBrowserFrameView -------------------------------------------------------

PanelBrowserFrameView::PanelBrowserFrameView(BrowserFrame* frame,
                                             PanelBrowserView* browser_view)
    : BrowserNonClientFrameView(frame, browser_view),
      panel_browser_view_(browser_view),
      paint_state_(NOT_PAINTED),
      settings_button_(NULL),
      close_button_(NULL),
      title_icon_(NULL),
      title_label_(NULL),
      is_settings_button_visible_(false) {
  EnsureResourcesInitialized();
  frame->set_frame_type(views::Widget::FRAME_TYPE_FORCE_CUSTOM);

  settings_button_ =  new views::MenuButton(NULL, string16(), this, false);
  settings_button_->SetIcon(*(settings_button_resources.normal_image));
  settings_button_->SetHoverIcon(*(settings_button_resources.hover_image));
  settings_button_->SetPushedIcon(*(settings_button_resources.pushed_image));
  settings_button_->set_alignment(views::TextButton::ALIGN_CENTER);
  settings_button_->set_border(NULL);
  settings_button_->SetTooltipText(
      l10n_util::GetStringUTF16(IDS_NEW_TAB_APP_SETTINGS));
  settings_button_->SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_NEW_TAB_APP_SETTINGS));
  settings_button_->SetVisible(is_settings_button_visible_);
  AddChildView(settings_button_);

  close_button_ = new views::ImageButton(this);
  close_button_->SetImage(views::CustomButton::BS_NORMAL,
                          close_button_resources.normal_image);
  close_button_->SetImage(views::CustomButton::BS_HOT,
                          close_button_resources.hover_image);
  close_button_->SetImage(views::CustomButton::BS_PUSHED,
                          close_button_resources.pushed_image);
  close_button_->SetTooltipText(
      l10n_util::GetStringUTF16(IDS_TOOLTIP_CLOSE_TAB));
  close_button_->SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_ACCNAME_CLOSE));
  AddChildView(close_button_);

  title_icon_ = new TabIconView(this);
  title_icon_->set_is_light(true);
  AddChildView(title_icon_);
  title_icon_->Update();

  title_label_ = new views::Label(GetTitleText());
  title_label_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  title_label_->SetAutoColorReadabilityEnabled(false);
  AddChildView(title_label_);

  mouse_watcher_.reset(new MouseWatcher(this));
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
  if (!bounds().Contains(point))
    return HTNOWHERE;

  int frame_component =
      frame()->client_view()->NonClientHitTest(point);
  if (frame_component != HTNOWHERE)
    return frame_component;

  if (close_button_->IsVisible() &&
      close_button_->GetMirroredBounds().Contains(point))
    return HTCLOSE;

  int window_component = GetHTComponentForFrame(point,
      NonClientBorderThickness(), NonClientBorderThickness(),
      0, 0,
      frame()->widget_delegate()->CanResize());
  // Fall back to the caption if no other component matches.
  return (window_component == HTNOWHERE) ? HTCAPTION : window_component;
}

void PanelBrowserFrameView::GetWindowMask(const gfx::Size& size,
                                          gfx::Path* window_mask) {
  // For panel, the window shape is rectangle with top-left and top-right
  // corners rounded.
  if (size.height() <= Panel::kMinimizedPanelHeight) {
    // For minimize panel, we need to produce the window mask applicable to
    // the 3-pixel lines.
    window_mask->moveTo(0, SkIntToScalar(size.height()));
    window_mask->lineTo(0, 1);
    window_mask->lineTo(1, 0);
    window_mask->lineTo(SkIntToScalar(size.width()) - 1, 0);
    window_mask->lineTo(SkIntToScalar(size.width()), 1);
    window_mask->lineTo(SkIntToScalar(size.width()),
                        SkIntToScalar(size.height()));
  } else {
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
  }
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
  PaintFrameBorder(canvas);
  PaintClientEdge(canvas);
}

void PanelBrowserFrameView::OnThemeChanged() {
  LoadImageResources();
}

gfx::Size PanelBrowserFrameView::GetMinimumSize() {
  // This makes the panel be able to shrink to very small, like 3-pixel lines.
  // Since the panel cannot be resized by the user, we do not need to enforce
  // the minimum size.
  return gfx::Size();
}

void PanelBrowserFrameView::Layout() {
  // Cancel the settings button animation if the layout of titlebar is being
  // updated.
  if (settings_button_animator_.get() && settings_button_animator_->IsShowing())
    settings_button_animator_->Reset();

  // Layout the close button.
  gfx::Size close_button_size = close_button_->GetPreferredSize();
  close_button_->SetBounds(
      width() - kFrameBorderThickness - kButtonSpacing -
          close_button_size.width(),
      (NonClientTopBorderHeight() - close_button_size.height()) / 2,
      close_button_size.width(),
      close_button_size.height());

  // Layout the settings button.
  gfx::Size settings_button_size = settings_button_->GetPreferredSize();
  settings_button_->SetBounds(
      close_button_->x() - kButtonSpacing - settings_button_size.width(),
      (NonClientTopBorderHeight() - settings_button_size.height()) / 2,
      settings_button_size.width(),
      settings_button_size.height());

  // Trace the full bounds and zero-size bounds for animation purpose.
  settings_button_full_bounds_ = settings_button_->bounds();
  settings_button_zero_bounds_.SetRect(
      settings_button_full_bounds_.x() +
          settings_button_full_bounds_.width() / 2,
      settings_button_full_bounds_.y() +
          settings_button_full_bounds_.height() / 2,
      0,
      0);

  // Layout the icon.
  int icon_y = (NonClientTopBorderHeight() - kIconSize) / 2;
  title_icon_->SetBounds(
      kFrameBorderThickness + kIconSpacing,
      icon_y,
      kIconSize,
      kIconSize);

  // Layout the title.
  int title_x = title_icon_->bounds().right() + kIconSpacing;
  int title_height = BrowserFrame::GetTitleFont().GetHeight();
  title_label_->SetBounds(
      title_x,
      icon_y + ((kIconSize - title_height - 1) / 2),
      std::max(0, settings_button_->x() - kButtonSpacing - title_x),
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
  if (event.IsOnlyLeftMouseButton() &&
      panel_browser_view_->OnTitlebarMousePressed(event.location())) {
    return true;
  }
  return BrowserNonClientFrameView::OnMousePressed(event);
}

bool PanelBrowserFrameView::OnMouseDragged(const views::MouseEvent& event) {
  if (panel_browser_view_->OnTitlebarMouseDragged(event.location()))
    return true;
  return BrowserNonClientFrameView::OnMouseDragged(event);
}

void PanelBrowserFrameView::OnMouseReleased(const views::MouseEvent& event) {
  if (panel_browser_view_->OnTitlebarMouseReleased())
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
  if (sender == close_button_)
    frame()->Close();
}

void PanelBrowserFrameView::RunMenu(View* source, const gfx::Point& pt) {
  if (!EnsureSettingsMenuCreated())
    return;

  DCHECK_EQ(settings_button_, source);
  gfx::Point screen_point;
  views::View::ConvertPointToScreen(source, &screen_point);
  if (settings_menu_runner_->RunMenuAt(source->GetWidget(),
          settings_button_, gfx::Rect(screen_point, source->size()),
          views::MenuItemView::TOPRIGHT, views::MenuRunner::HAS_MNEMONICS) ==
      views::MenuRunner::MENU_DELETED)
    return;
}

bool PanelBrowserFrameView::ShouldTabIconViewAnimate() const {
  // This function is queried during the creation of the window as the
  // TabIconView we host is initialized, so we need to NULL check the selected
  // TabContents because in this condition there is not yet a selected tab.
  TabContents* current_tab = browser_view()->GetSelectedTabContents();
  return current_tab ? current_tab->IsLoading() : false;
}

SkBitmap PanelBrowserFrameView::GetFaviconForTabIconView() {
  return frame()->widget_delegate()->GetWindowIcon();
}

void PanelBrowserFrameView::AnimationEnded(const ui::Animation* animation) {
  settings_button_->SetVisible(is_settings_button_visible_);
}

void PanelBrowserFrameView::AnimationProgressed(
    const ui::Animation* animation) {
  gfx::Rect animation_start_bounds, animation_end_bounds;
  if (is_settings_button_visible_) {
    animation_start_bounds = settings_button_zero_bounds_;
    animation_end_bounds = settings_button_full_bounds_;
  } else {
    animation_start_bounds = settings_button_full_bounds_;
    animation_end_bounds = settings_button_zero_bounds_;
  }
  gfx::Rect new_bounds = settings_button_animator_->CurrentValueBetween(
      animation_start_bounds, animation_end_bounds);
  if (new_bounds == animation_end_bounds)
    AnimationEnded(animation);
  else
    settings_button_->SetBoundsRect(new_bounds);
}

void PanelBrowserFrameView::AnimationCanceled(const ui::Animation* animation) {
  AnimationEnded(animation);
}

int PanelBrowserFrameView::NonClientBorderThickness() const {
  return kFrameBorderThickness + kClientEdgeThickness;
}

int PanelBrowserFrameView::NonClientTopBorderHeight() const {
  return kFrameBorderThickness + kTitlebarHeight + kClientEdgeThickness;
}

gfx::Size PanelBrowserFrameView::NonClientAreaSize() const {
  return gfx::Size(NonClientBorderThickness() * 2,
                   NonClientTopBorderHeight() + NonClientBorderThickness());
}

SkColor PanelBrowserFrameView::GetTitleColor(PaintState paint_state) const {
  switch (paint_state) {
    case PAINT_AS_INACTIVE:
      return GetThemeProvider()->GetColor(
          ThemeService::COLOR_BACKGROUND_TAB_TEXT);
    case PAINT_AS_ACTIVE:
      return GetThemeProvider()->GetColor(ThemeService::COLOR_TAB_TEXT);
    case PAINT_FOR_ATTENTION:
      return kTitleTextColorForAttention;
    default:
      NOTREACHED();
      return SkColor();
  }
}

gfx::Font* PanelBrowserFrameView::GetTitleFont() const {
  return title_font;
}

SkBitmap* PanelBrowserFrameView::GetFrameTheme(PaintState paint_state) const {
  switch (paint_state) {
    case PAINT_AS_INACTIVE:
      return GetThemeProvider()->GetBitmapNamed(IDR_THEME_TAB_BACKGROUND);
    case PAINT_AS_ACTIVE:
      return GetThemeProvider()->GetBitmapNamed(IDR_THEME_TOOLBAR);
    case PAINT_FOR_ATTENTION:
      return background_bitmap_for_attention;
    default:
      NOTREACHED();
      return NULL;
  }
}

void PanelBrowserFrameView::UpdateControlStyles(PaintState paint_state) {
  DCHECK(paint_state != NOT_PAINTED);

  if (paint_state == paint_state_)
    return;
  paint_state_ = paint_state;

  SkColor title_color = GetTitleColor(paint_state_);
  title_label_->SetEnabledColor(title_color);
  title_label_->SetFont(*GetTitleFont());

  close_button_->SetBackground(title_color,
                               close_button_resources.normal_image,
                               close_button_resources.mask_image);
}

void PanelBrowserFrameView::PaintFrameBorder(gfx::Canvas* canvas) {
  SkBitmap* theme_frame = GetFrameTheme(paint_state_);

  // Draw the theme frame.
  canvas->TileImageInt(*theme_frame, 0, 0, width(), height());

  // No need to paint other stuff if panel is minimized.
  if (height() <= Panel::kMinimizedPanelHeight)
    return;

  // Draw the top border.
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
}

void PanelBrowserFrameView::PaintClientEdge(gfx::Canvas* canvas) {
  int client_area_top = client_view_bounds_.y();

  // No need to paint other stuff if panel is minimized.
  if (height() <= Panel::kMinimizedPanelHeight)
    return;

  // Draw the top edge.
  int top_edge_y = client_area_top - client_edges.top->height();
  canvas->DrawBitmapInt(
      *(client_edges.top_left),
      client_view_bounds_.x() - client_edges.top_left->width(),
      top_edge_y);
  canvas->TileImageInt(
      *(client_edges.top), client_view_bounds_.x(), top_edge_y,
      client_view_bounds_.width(), client_edges.top->height());
  canvas->DrawBitmapInt(
      *(client_edges.top_right), client_view_bounds_.right(), top_edge_y);

  // Draw the right edge.
  int client_area_bottom =
      std::max(client_area_top, client_view_bounds_.bottom());
  int client_area_height = client_area_bottom - client_area_top;
  canvas->TileImageInt(
      *(client_edges.right), client_view_bounds_.right(), client_area_top,
      client_edges.right->width(), client_area_height);

  // Draw the bottom edge.
  canvas->DrawBitmapInt(
      *(client_edges.bottom_right), client_view_bounds_.right(),
      client_area_bottom);
  canvas->TileImageInt(
      *(client_edges.bottom), client_view_bounds_.x(), client_area_bottom,
      client_view_bounds_.width(), client_edges.bottom_right->height());
  canvas->DrawBitmapInt(
      *(client_edges.bottom_left),
      client_view_bounds_.x() - client_edges.bottom_left->width(),
      client_area_bottom);

  // Draw the left edge.
  canvas->TileImageInt(
      *(client_edges.left),
      client_view_bounds_.x() - client_edges.left->width(),
      client_area_top, client_edges.left->width(), client_area_height);
}

string16 PanelBrowserFrameView::GetTitleText() const {
  return frame()->widget_delegate()->GetWindowTitle();
}

void PanelBrowserFrameView::UpdateTitleBar() {
  title_label_->SetText(GetTitleText());
}

void PanelBrowserFrameView::OnFocusChanged(bool focused) {
  UpdateSettingsButtonVisibility(focused,
                                 mouse_watcher_->IsCursorInViewBounds());
  SchedulePaint();
}

void PanelBrowserFrameView::OnMouseEnterOrLeaveWindow(bool mouse_entered) {
  // Panel might be closed when we still watch the mouse event.
  if (!panel_browser_view_->panel())
    return;
  UpdateSettingsButtonVisibility(panel_browser_view_->focused(),
                                 mouse_entered);
}

void PanelBrowserFrameView::UpdateSettingsButtonVisibility(
    bool focused, bool cursor_in_view) {
  bool is_settings_button_visible = focused || cursor_in_view;
  if (is_settings_button_visible_ == is_settings_button_visible)
    return;
  is_settings_button_visible_ = is_settings_button_visible;

  // Even if we're hidng the settings button, we still make it visible for the
  // time period that the animation is running.
  settings_button_->SetVisible(true);

  if (settings_button_animator_.get()) {
    if (settings_button_animator_->IsShowing())
      settings_button_animator_->Reset();
  } else {
    settings_button_animator_.reset(new ui::SlideAnimation(this));
    settings_button_animator_->SetTweenType(ui::Tween::LINEAR);
    settings_button_animator_->SetSlideDuration(
        kUpdateSettingsVisibilityAnimationMs);
  }

  settings_button_animator_->Show();
}

bool PanelBrowserFrameView::EnsureSettingsMenuCreated() {
  if (settings_menu_runner_.get())
    return true;

  const Extension* extension = panel_browser_view_->panel()->GetExtension();
  if (!extension)
    return false;

  settings_menu_model_.reset(
      new PanelSettingsMenuModel(panel_browser_view_->panel()));
  settings_menu_adapter_.reset(
      new views::MenuModelAdapter(settings_menu_model_.get()));
  settings_menu_ = new views::MenuItemView(settings_menu_adapter_.get());
  settings_menu_adapter_->BuildMenu(settings_menu_);
  settings_menu_runner_.reset(new views::MenuRunner(settings_menu_));
  return true;
}
