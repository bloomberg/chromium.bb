// Copyright (c) 2011 The Chromium Authors. All rights reserved.
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
#include "ui/gfx/skia_util.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/menu_model_adapter.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/widget/widget_delegate.h"
#include "views/painter.h"

namespace {

// The height in pixels of the titlebar.
const int kTitlebarHeight = 24;

// The thickness in pixels of the border.
const int kBorderThickness = 1;

// No client edge is present.
const int kPanelClientEdgeThickness = 0;

// The spacing in pixels between the icon and the left border.
const int kIconAndBorderSpacing = 4;

// The height and width in pixels of the icon.
const int kIconSize = 16;

// The spacing in pixels between the title and the icon on the left, or the
// button on the right.
const int kTitleSpacing = 8;

// The spacing in pixels between the close button and the right border.
const int kCloseButtonAndBorderSpacing = 8;

// The spacing in pixels between the close button and the settings button.
const int kSettingsButtonAndCloseButtonSpacing = 8;

// This value is experimental and subjective.
const int kUpdateSettingsVisibilityAnimationMs = 120;

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
  SkBitmap* mask_image;
  SkBitmap* hover_image;
  SkBitmap* pushed_image;

  ButtonResources(int normal_image_id, int mask_image_id, int hover_image_id,
                  int pushed_image_id)
      : normal_image(NULL),
        mask_image(NULL),
        hover_image(NULL),
        pushed_image(NULL) {
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

SkPaint* CreateGradientPaint(SkColor start_color, SkColor end_color) {
  SkShader* shader = gfx::CreateGradientShader(
      0, kTitlebarHeight, start_color, end_color);
  SkPaint* paint = new SkPaint();
  paint->setStyle(SkPaint::kFill_Style);
  paint->setAntiAlias(true);
  paint->setShader(shader);
  shader->unref();
  return paint;
}

const ButtonResources& GetSettingsButtonResources() {
  static ButtonResources* buttons = NULL;
  if (!buttons) {
    buttons = new ButtonResources(IDR_BALLOON_WRENCH, 0,
                                  IDR_BALLOON_WRENCH_H, IDR_BALLOON_WRENCH_P);
  }
  return *buttons;
}

const ButtonResources& GetCloseButtonResources() {
  static ButtonResources* buttons = NULL;
  if (!buttons) {
    buttons = new ButtonResources(IDR_TAB_CLOSE, IDR_TAB_CLOSE_MASK,
                                  IDR_TAB_CLOSE_H, IDR_TAB_CLOSE_P);
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

const gfx::Font& GetTitleFont() {
  static gfx::Font* font = NULL;
  if (!font) {
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    font = new gfx::Font(rb.GetFont(ResourceBundle::BoldFont));
  }
  return *font;
}

const SkPaint& GetActiveBackgroundDefaultPaint() {
  static SkPaint* paint = NULL;
  if (!paint) {
    paint = CreateGradientPaint(kActiveBackgroundDefaultColorStart,
                                kActiveBackgroundDefaultColorEnd);
  }
  return *paint;
}

const SkPaint& GetInactiveBackgroundDefaultPaint() {
  static SkPaint* paint = NULL;
  if (!paint) {
    paint = CreateGradientPaint(kInactiveBackgroundDefaultColorStart,
                                kInactiveBackgroundDefaultColorEnd);
  }
  return *paint;
}

const SkPaint& GetAttentionBackgroundDefaultPaint() {
  static SkPaint* paint = NULL;
  if (!paint) {
    paint = CreateGradientPaint(kAttentionBackgroundDefaultColorStart,
                                kAttentionBackgroundDefaultColorEnd);
  }
  return *paint;
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
  frame->set_frame_type(views::Widget::FRAME_TYPE_FORCE_CUSTOM);

  const ButtonResources& settings_button_resources =
      GetSettingsButtonResources();
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

  const ButtonResources& close_button_resources = GetCloseButtonResources();
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
  PaintFrameBorder(canvas);
}

void PanelBrowserFrameView::OnThemeChanged() {
}

gfx::Size PanelBrowserFrameView::GetMinimumSize() {
  // This makes the panel be able to shrink to very small, like 4-pixel lines.
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
      width() - kBorderThickness - kCloseButtonAndBorderSpacing -
          close_button_size.width(),
      (NonClientTopBorderHeight() - close_button_size.height()) / 2,
      close_button_size.width(),
      close_button_size.height());

  // Layout the settings button.
  gfx::Size settings_button_size = settings_button_->GetPreferredSize();
  settings_button_->SetBounds(
      close_button_->x() - kSettingsButtonAndCloseButtonSpacing -
          settings_button_size.width(),
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
      kBorderThickness + kIconAndBorderSpacing,
      icon_y,
      kIconSize,
      kIconSize);

  // Layout the title.
  int title_x = title_icon_->bounds().right() + kTitleSpacing;
  int title_height = BrowserFrame::GetTitleFont().GetHeight();
  title_label_->SetBounds(
      title_x,
      icon_y + ((kIconSize - title_height - 1) / 2),
      std::max(0, settings_button_->x() - kTitleSpacing - title_x),
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
  return kBorderThickness + kPanelClientEdgeThickness;
}

int PanelBrowserFrameView::NonClientTopBorderHeight() const {
  return kBorderThickness + kTitlebarHeight + kPanelClientEdgeThickness;
}

gfx::Size PanelBrowserFrameView::NonClientAreaSize() const {
  return gfx::Size(NonClientBorderThickness() * 2,
                   NonClientTopBorderHeight() + NonClientBorderThickness());
}

bool PanelBrowserFrameView::UsingDefaultTheme() const {
  ThemeService* theme_service = ThemeServiceFactory::GetForProfile(
      panel_browser_view_->panel()->browser()->profile());
  return theme_service->UsingDefaultTheme();
}

SkColor PanelBrowserFrameView::GetDefaultTitleColor(
    PaintState paint_state) const {
  switch (paint_state) {
    case PAINT_AS_INACTIVE:
      return kActiveTitleTextDefaultColor;
    case PAINT_AS_ACTIVE:
      return kInactiveTitleTextDefaultColor;
    case PAINT_FOR_ATTENTION:
      return kAttentionTitleTextDefaultColor;
    default:
      NOTREACHED();
      return SkColor();
  }
}

SkColor PanelBrowserFrameView::GetTitleColor(PaintState paint_state) const {
  switch (paint_state) {
    case PAINT_AS_INACTIVE:
      return SkColorSetA(
          GetThemeProvider()->GetColor(ThemeService::COLOR_BACKGROUND_TAB_TEXT),
          kInactiveAlphaBlending);
    case PAINT_AS_ACTIVE:
      return GetThemeProvider()->GetColor(ThemeService::COLOR_TAB_TEXT);
    case PAINT_FOR_ATTENTION:
      return kAttentionTitleTextDefaultColor;
    default:
      NOTREACHED();
      return SkColor();
  }
}

const SkPaint& PanelBrowserFrameView::GetDefaultFrameTheme(
    PaintState paint_state) const {
  switch (paint_state) {
    case PAINT_AS_INACTIVE:
      return GetInactiveBackgroundDefaultPaint();
    case PAINT_AS_ACTIVE:
      return GetActiveBackgroundDefaultPaint();
    case PAINT_FOR_ATTENTION:
      return GetAttentionBackgroundDefaultPaint();
    default:
      NOTREACHED();
      return GetInactiveBackgroundDefaultPaint();
  }
}

SkBitmap* PanelBrowserFrameView::GetFrameTheme(PaintState paint_state) const {
  switch (paint_state) {
    case PAINT_AS_INACTIVE:
      return GetThemeProvider()->GetBitmapNamed(IDR_THEME_TAB_BACKGROUND);
    case PAINT_AS_ACTIVE:
      return GetThemeProvider()->GetBitmapNamed(IDR_THEME_TOOLBAR);
    case PAINT_FOR_ATTENTION:
      // Background color for drawing attention is same regardless of the
      // theme. GetDefaultFrameTheme should be used.
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
  title_label_->SetFont(GetTitleFont());

  close_button_->SetBackground(title_color,
                               GetCloseButtonResources().normal_image,
                               GetCloseButtonResources().mask_image);
}

void PanelBrowserFrameView::PaintFrameBorder(gfx::Canvas* canvas) {
  // Paint the background.
  if (paint_state_ == PAINT_FOR_ATTENTION || UsingDefaultTheme()) {
    const SkPaint& paint = GetDefaultFrameTheme(paint_state_);
    canvas->DrawRectInt(0, 0, width(), kTitlebarHeight, paint);
  } else {
    SkBitmap* bitmap = GetFrameTheme(paint_state_);
    canvas->TileImageInt(*bitmap, 0, 0, width(), kTitlebarHeight);
  }

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
  if (height() > kTitlebarHeight) {
    canvas->DrawRectInt(kDividerColor, kBorderThickness, kTitlebarHeight,
                        width() - 1 - 2 * kBorderThickness, kBorderThickness);
  }
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
