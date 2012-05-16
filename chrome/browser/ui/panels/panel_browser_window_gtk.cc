// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/panels/panel_browser_window_gtk.h"

#include "base/bind.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/gtk/browser_titlebar.h"
#include "chrome/browser/ui/gtk/custom_button.h"
#include "chrome/browser/ui/gtk/gtk_theme_service.h"
#include "chrome/browser/ui/gtk/nine_box.h"
#include "chrome/browser/ui/panels/panel.h"
#include "chrome/browser/ui/panels/panel_bounds_animation.h"
#include "chrome/browser/ui/panels/panel_browser_titlebar_gtk.h"
#include "chrome/browser/ui/panels/panel_drag_gtk.h"
#include "chrome/browser/ui/panels/panel_manager.h"
#include "chrome/browser/ui/panels/panel_strip.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/notification_service.h"
#include "grit/theme_resources_standard.h"
#include "grit/ui_resources.h"
#include "third_party/skia/include/core/SkShader.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/cairo_cached_surface.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/skia_util.h"

using content::WebContents;

namespace {

// Colors used to draw titlebar and frame for drawing attention under default
// theme. It is also used in non-default theme since attention color is not
// defined in the theme.
const SkColor kAttentionBackgroundColorStart = SkColorSetRGB(0xff, 0xab, 0x57);
const SkColor kAttentionBackgroundColorEnd = SkColorSetRGB(0xe6, 0x9a, 0x4e);

// Set minimium width for window really small.
const int kMinWindowWidth = 26;

gfx::Image* CreateGradientImage(SkColor start_color, SkColor end_color) {
  // Though the height of titlebar, used for creating gradient, cannot be
  // pre-determined, we use a reasonably bigger value that is obtained from
  // the experimentation and should work for most cases.
  const int gradient_size = 32;
  SkShader* shader = gfx::CreateGradientShader(
      0, gradient_size, start_color, end_color);
  SkPaint paint;
  paint.setStyle(SkPaint::kFill_Style);
  paint.setAntiAlias(true);
  paint.setShader(shader);
  shader->unref();
  gfx::Canvas canvas(gfx::Size(1, gradient_size), true);
  canvas.DrawRect(gfx::Rect(0, 0, 1, gradient_size), paint);
  return new gfx::Image(canvas.ExtractBitmap());
}

gfx::Image* GetAttentionBackgroundImage() {
  static gfx::Image* image = NULL;
  if (!image) {
    image = CreateGradientImage(kAttentionBackgroundColorStart,
                                kAttentionBackgroundColorEnd);
  }
  return image;
}

}  // namespace

NativePanel* Panel::CreateNativePanel(Browser* browser, Panel* panel,
                                      const gfx::Rect& bounds) {
  PanelBrowserWindowGtk* panel_browser_window_gtk =
      new PanelBrowserWindowGtk(browser, panel, bounds);
  panel_browser_window_gtk->Init();
  return panel_browser_window_gtk;
}

PanelBrowserWindowGtk::PanelBrowserWindowGtk(Browser* browser,
                                             Panel* panel,
                                             const gfx::Rect& bounds)
    : BrowserWindowGtk(browser),
      panel_(panel),
      bounds_(bounds),
      is_drawing_attention_(false) {
}

PanelBrowserWindowGtk::~PanelBrowserWindowGtk() {
}

void PanelBrowserWindowGtk::Init() {
  BrowserWindowGtk::Init();

  // Keep the window always on top.
  gtk_window_set_keep_above(window(), TRUE);

  // Show the window on all the virtual desktops.
  gtk_window_stick(window());

  // Do not show an icon in the task bar.  Window operations such as close,
  // minimize etc. can only be done from the panel UI.
  gtk_window_set_skip_taskbar_hint(window(), TRUE);

  // Only need to watch for titlebar button release event. BrowserWindowGtk
  // already watches for button-press-event and determines if titlebar or
  // window edge was hit.
  g_signal_connect(titlebar_widget(), "button-release-event",
                   G_CALLBACK(OnTitlebarButtonReleaseEventThunk), this);

  registrar_.Add(
      this,
      chrome::NOTIFICATION_WINDOW_CLOSED,
      content::Source<GtkWindow>(window()));
}

bool PanelBrowserWindowGtk::ShouldDrawContentDropShadow() const {
  return !panel_->IsMinimized();
}

BrowserTitlebar* PanelBrowserWindowGtk::CreateBrowserTitlebar() {
  return new PanelBrowserTitlebarGtk(this, window());
}

PanelBrowserTitlebarGtk* PanelBrowserWindowGtk::GetPanelTitlebar() const {
  return static_cast<PanelBrowserTitlebarGtk*>(titlebar());
}

PanelBrowserWindowGtk::PaintState PanelBrowserWindowGtk::GetPaintState() const {
  if (is_drawing_attention_)
    return PAINT_FOR_ATTENTION;
  return IsActive() ? PAINT_AS_ACTIVE : PAINT_AS_INACTIVE;
}

bool PanelBrowserWindowGtk::GetWindowEdge(int x, int y, GdkWindowEdge* edge) {
  // Only detect the window edge when panels can be resized by the user.
  // This method is used by the base class to detect when the cursor has
  // hit the window edge in order to change the cursor to a resize cursor
  // and to detect when to initiate a resize drag.
  panel::Resizability resizability = panel_->CanResizeByMouse();
  if (panel::NOT_RESIZABLE == resizability)
    return false;

  if (!BrowserWindowGtk::GetWindowEdge(x, y, edge))
    return FALSE;

  // Special handling if bottom edge is not resizable.
  if (panel::RESIZABLE_ALL_SIDES_EXCEPT_BOTTOM == resizability) {
    if (*edge == GDK_WINDOW_EDGE_SOUTH)
      return FALSE;
    if (*edge == GDK_WINDOW_EDGE_SOUTH_WEST)
      *edge = GDK_WINDOW_EDGE_WEST;
    else if (*edge == GDK_WINDOW_EDGE_SOUTH_EAST)
      *edge = GDK_WINDOW_EDGE_EAST;
  }

  return TRUE;
}

GdkRegion* PanelBrowserWindowGtk::GetWindowShape(int width, int height) const {
  // For panels, only top corners are rounded. The bottom corners are not
  // rounded because panels are aligned to the bottom edge of the screen.
  GdkRectangle top_top_rect = { 3, 0, width - 6, 1 };
  GdkRectangle top_mid_rect = { 1, 1, width - 2, 2 };
  GdkRectangle mid_rect = { 0, 3, width, height - 3 };
  GdkRegion* mask = gdk_region_rectangle(&top_top_rect);
  gdk_region_union_with_rect(mask, &top_mid_rect);
  gdk_region_union_with_rect(mask, &mid_rect);
  return mask;
}

void PanelBrowserWindowGtk::DrawCustomFrameBorder(GtkWidget* widget) {
  static NineBox* custom_frame_border = NULL;
  if (!custom_frame_border) {
    custom_frame_border = new NineBox(IDR_WINDOW_TOP_LEFT_CORNER,
                                      IDR_WINDOW_TOP_CENTER,
                                      IDR_WINDOW_TOP_RIGHT_CORNER,
                                      IDR_WINDOW_LEFT_SIDE,
                                      0,
                                      IDR_WINDOW_RIGHT_SIDE,
                                      IDR_PANEL_BOTTOM_LEFT_CORNER,
                                      IDR_WINDOW_BOTTOM_CENTER,
                                      IDR_PANEL_BOTTOM_RIGHT_CORNER);
  }
  custom_frame_border->RenderToWidget(widget);
}

void PanelBrowserWindowGtk::EnsureDragHelperCreated() {
  if (drag_helper_.get())
    return;

  drag_helper_.reset(new PanelDragGtk(panel_.get()));
  gtk_box_pack_end(GTK_BOX(window_vbox_), drag_helper_->widget(),
                   FALSE, FALSE, 0);
}

bool PanelBrowserWindowGtk::HandleTitleBarLeftMousePress(
    GdkEventButton* event,
    guint32 last_click_time,
    gfx::Point last_click_position) {
  DCHECK_EQ(1U, event->button);
  DCHECK_EQ(GDK_BUTTON_PRESS, event->type);

  EnsureDragHelperCreated();
  drag_helper_->InitialTitlebarMousePress(event, titlebar_widget());
  return TRUE;
}

bool PanelBrowserWindowGtk::HandleWindowEdgeLeftMousePress(
    GtkWindow* window,
    GdkWindowEdge edge,
    GdkEventButton* event) {
  DCHECK_EQ(1U, event->button);
  DCHECK_EQ(GDK_BUTTON_PRESS, event->type);

  EnsureDragHelperCreated();
  // Resize cursor was set by BrowserWindowGtk when mouse moved over
  // window edge.
  GdkCursor* cursor =
      gdk_window_get_cursor(gtk_widget_get_window(GTK_WIDGET(window_)));
  drag_helper_->InitialWindowEdgeMousePress(event, cursor, edge);
  return TRUE;
}

void PanelBrowserWindowGtk::SaveWindowPosition() {
  // We don't save window position for panels as it's controlled by
  // PanelManager.
  return;
}

void PanelBrowserWindowGtk::SetGeometryHints() {
  // Set minimum height the window can be set to.
  GdkGeometry hints;
  hints.min_height = Panel::kMinimizedPanelHeight;
  hints.min_width = kMinWindowWidth;
  gtk_window_set_geometry_hints(
      window(), GTK_WIDGET(window()), &hints, GDK_HINT_MIN_SIZE);

  DCHECK(frame_size_.IsEmpty());
}

void PanelBrowserWindowGtk::SetBounds(const gfx::Rect& bounds) {
  // This should never be called.
  DLOG(WARNING) << "Unexpected call to PanelBrowserWindowGtk::SetBounds()";
}

void PanelBrowserWindowGtk::OnSizeChanged(int width, int height) {
  BrowserWindowGtk::OnSizeChanged(width, height);

  if (!frame_size_.IsEmpty())
    return;

  // The system is allowed to size the window before the desired panel
  // bounds are applied. Save the frame size allocated by the system as
  // it will be affected when we shrink the panel smaller than the frame.
  frame_size_ = GetNonClientFrameSize();

  int top = bounds_.bottom() - height;
  int left = bounds_.right() - width;

  gtk_window_move(window_, left, top);
  StartBoundsAnimation(gfx::Rect(left, top, width, height), bounds_);
  panel_->OnWindowSizeAvailable();

  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_PANEL_WINDOW_SIZE_KNOWN,
      content::Source<Panel>(panel_.get()),
      content::NotificationService::NoDetails());
}

bool PanelBrowserWindowGtk::UseCustomFrame() const {
  // We always use custom frame for panels.
  return true;
}

bool PanelBrowserWindowGtk::UsingCustomPopupFrame() const {
  // We do not draw custom popup frame.
  return false;
}

void PanelBrowserWindowGtk::DrawPopupFrame(cairo_t* cr,
                                           GtkWidget* widget,
                                           GdkEventExpose* event) {
  NOTREACHED();
}

const gfx::Image* PanelBrowserWindowGtk::GetThemeFrameImage() const {
  PaintState paint_state = GetPaintState();
  if (paint_state == PAINT_FOR_ATTENTION)
    return GetAttentionBackgroundImage();

  GtkThemeService* theme_provider = GtkThemeService::GetFrom(
      browser()->profile());
  if (theme_provider->UsingDefaultTheme()) {
    // We choose to use the window frame theme to paint panels for the default
    // theme. This is because the default tab theme does not work well for the
    // user to recognize active and inactive panels.
    return theme_provider->GetImageNamed(paint_state == PAINT_AS_ACTIVE ?
        IDR_THEME_FRAME : IDR_THEME_FRAME_INACTIVE);
  }

  return theme_provider->GetImageNamed(paint_state == PAINT_AS_ACTIVE ?
      IDR_THEME_TOOLBAR : IDR_THEME_TAB_BACKGROUND);
}

void PanelBrowserWindowGtk::DrawCustomFrame(cairo_t* cr,
                                            GtkWidget* widget,
                                            GdkEventExpose* event) {
  gfx::CairoCachedSurface* surface = GetThemeFrameImage()->ToCairo();

  surface->SetSource(cr, widget, 0, 0);
  cairo_pattern_set_extend(cairo_get_source(cr), CAIRO_EXTEND_REPEAT);
  cairo_rectangle(cr, event->area.x, event->area.y,
                  event->area.width, event->area.height);
  cairo_fill(cr);
}

void PanelBrowserWindowGtk::ActiveWindowChanged(GdkWindow* active_window) {
  bool was_active = IsActive();
  BrowserWindowGtk::ActiveWindowChanged(active_window);
  bool is_active = IsActive();
  if (!window() || was_active == is_active)  // State didn't change.
    return;

  panel_->OnActiveStateChanged(is_active);
}

void PanelBrowserWindowGtk::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_WINDOW_CLOSED:
      // Cleanup.
      if (bounds_animator_.get())
        bounds_animator_.reset();

      if (drag_helper_.get())
        drag_helper_.reset();

      panel_->OnNativePanelClosed();
      break;
  }

  BrowserWindowGtk::Observe(type, source, details);
}

void PanelBrowserWindowGtk::ShowPanel() {
  Show();
}

void PanelBrowserWindowGtk::ShowPanelInactive() {
  ShowInactive();
}

gfx::Rect PanelBrowserWindowGtk::GetPanelBounds() const {
  return bounds_;
}

void PanelBrowserWindowGtk::SetPanelBounds(const gfx::Rect& bounds) {
  SetBoundsInternal(bounds, true);
}

void PanelBrowserWindowGtk::SetPanelBoundsInstantly(const gfx::Rect& bounds) {
  SetBoundsInternal(bounds, false);
}

void PanelBrowserWindowGtk::SetBoundsInternal(const gfx::Rect& bounds,
                                              bool animate) {
  if (bounds == bounds_)
    return;

  if (!animate) {
    // If no animation is in progress, apply bounds change instantly. Otherwise,
    // continue the animation with new target bounds.
    if (!IsAnimatingBounds())
      gdk_window_move_resize(gtk_widget_get_window(GTK_WIDGET(window())),
                             bounds.x(), bounds.y(),
                             bounds.width(), bounds.height());
  } else if (!frame_size_.IsEmpty()) {
    StartBoundsAnimation(bounds_, bounds);
  } else {
    // Wait until the window frame has been sized before starting the animation
    // to avoid animating the window before it is shown.
  }

  bounds_ = bounds;
}

void PanelBrowserWindowGtk::ClosePanel() {
  Close();
}

void PanelBrowserWindowGtk::ActivatePanel() {
  Activate();
}

void PanelBrowserWindowGtk::DeactivatePanel() {
  BrowserWindow* browser_window =
      panel_->manager()->GetNextBrowserWindowToActivate(panel_.get());
  if (browser_window) {
    browser_window->Activate();
  } else {
    Deactivate();
  }
}

bool PanelBrowserWindowGtk::IsPanelActive() const {
  return IsActive();
}

void PanelBrowserWindowGtk::PreventActivationByOS(bool prevent_activation) {
  gtk_window_set_accept_focus(window(), !prevent_activation);
  return;
}

gfx::NativeWindow PanelBrowserWindowGtk::GetNativePanelHandle() {
  return GetNativeHandle();
}

void PanelBrowserWindowGtk::UpdatePanelTitleBar() {
  UpdateTitleBar();
}

void PanelBrowserWindowGtk::UpdatePanelLoadingAnimations(bool should_animate) {
  UpdateLoadingAnimations(should_animate);
}

void PanelBrowserWindowGtk::ShowTaskManagerForPanel() {
  ShowTaskManager();
}

FindBar* PanelBrowserWindowGtk::CreatePanelFindBar() {
  return CreateFindBar();
}

void PanelBrowserWindowGtk::NotifyPanelOnUserChangedTheme() {
  UserChangedTheme();
}

void PanelBrowserWindowGtk::PanelWebContentsFocused(WebContents* contents) {
  WebContentsFocused(contents);
}

void PanelBrowserWindowGtk::PanelCut() {
  Cut();
}

void PanelBrowserWindowGtk::PanelCopy() {
  Copy();
}

void PanelBrowserWindowGtk::PanelPaste() {
  Paste();
}

void PanelBrowserWindowGtk::DrawAttention(bool draw_attention) {
  DCHECK((panel_->attention_mode() & Panel::USE_PANEL_ATTENTION) != 0);

  if (is_drawing_attention_ == draw_attention)
    return;

  is_drawing_attention_ = draw_attention;

  GetPanelTitlebar()->UpdateTextColor();
  InvalidateWindow();

  if ((panel_->attention_mode() & Panel::USE_SYSTEM_ATTENTION) != 0)
    ::BrowserWindowGtk::FlashFrame(draw_attention);
}

bool PanelBrowserWindowGtk::IsDrawingAttention() const {
  return is_drawing_attention_;
}

bool PanelBrowserWindowGtk::PreHandlePanelKeyboardEvent(
    const NativeWebKeyboardEvent& event,
    bool* is_keyboard_shortcut) {
  return PreHandleKeyboardEvent(event, is_keyboard_shortcut);
}

void PanelBrowserWindowGtk::FullScreenModeChanged(bool is_full_screen) {
  // Nothing to do here as z-order rules for panels ensures that they're below
  // any app running in full screen mode.
}

void PanelBrowserWindowGtk::HandlePanelKeyboardEvent(
    const NativeWebKeyboardEvent& event) {
  HandleKeyboardEvent(event);
}

Browser* PanelBrowserWindowGtk::GetPanelBrowser() const {
  return browser();
}

void PanelBrowserWindowGtk::DestroyPanelBrowser() {
  DestroyBrowser();
}

void PanelBrowserWindowGtk::EnsurePanelFullyVisible() {
  gtk_window_present(window());
}

void PanelBrowserWindowGtk::SetPanelAlwaysOnTop(bool on_top) {
  gtk_window_set_keep_above(window(), on_top);
}

void PanelBrowserWindowGtk::EnableResizeByMouse(bool enable) {
}

void PanelBrowserWindowGtk::UpdatePanelMinimizeRestoreButtonVisibility() {
  GetPanelTitlebar()->UpdateMinimizeRestoreButtonVisibility();
}

gfx::Size PanelBrowserWindowGtk::WindowSizeFromContentSize(
    const gfx::Size& content_size) const {
  return gfx::Size(content_size.width() + frame_size_.width(),
                   content_size.height() + frame_size_.height());
}

gfx::Size PanelBrowserWindowGtk::ContentSizeFromWindowSize(
    const gfx::Size& window_size) const {
  return gfx::Size(window_size.width() - frame_size_.width(),
                   window_size.height() - frame_size_.height());
}

int PanelBrowserWindowGtk::TitleOnlyHeight() const {
  GtkAllocation allocation;
  gtk_widget_get_allocation(titlebar_widget(), &allocation);
  return allocation.height;
}

void PanelBrowserWindowGtk::StartBoundsAnimation(
    const gfx::Rect& from_bounds, const gfx::Rect& to_bounds) {
  animation_start_bounds_ = IsAnimatingBounds() ?
      last_animation_progressed_bounds_ : from_bounds;

  bounds_animator_.reset(new PanelBoundsAnimation(
      this, panel_.get(), animation_start_bounds_, to_bounds));

  bounds_animator_->Start();
  last_animation_progressed_bounds_ = animation_start_bounds_;
}

bool PanelBrowserWindowGtk::IsAnimatingBounds() const {
  return bounds_animator_.get() && bounds_animator_->is_animating();
}

void PanelBrowserWindowGtk::AnimationEnded(const ui::Animation* animation) {
  GetPanelTitlebar()->SendEnterNotifyToCloseButtonIfUnderMouse();
  panel_->manager()->OnPanelAnimationEnded(panel_.get());
}

void PanelBrowserWindowGtk::AnimationProgressed(
    const ui::Animation* animation) {
  DCHECK(!frame_size_.IsEmpty());

  gfx::Rect new_bounds = bounds_animator_->CurrentValueBetween(
      animation_start_bounds_, bounds_);

  gdk_window_move_resize(gtk_widget_get_window(GTK_WIDGET(window())),
                         new_bounds.x(), new_bounds.y(),
                         new_bounds.width(), new_bounds.height());

  last_animation_progressed_bounds_ = new_bounds;
}

gboolean PanelBrowserWindowGtk::OnTitlebarButtonReleaseEvent(
    GtkWidget* widget, GdkEventButton* event) {
  if (event->button != 1)
    return TRUE;

  panel_->OnTitlebarClicked((event->state & GDK_CONTROL_MASK) ?
                            panel::APPLY_TO_ALL : panel::NO_MODIFIER);
  return TRUE;
}

// NativePanelTesting implementation.
class NativePanelTestingGtk : public NativePanelTesting {
 public:
  explicit NativePanelTestingGtk(
      PanelBrowserWindowGtk* panel_browser_window_gtk);

 private:
  virtual void PressLeftMouseButtonTitlebar(
      const gfx::Point& mouse_location, panel::ClickModifier modifier) OVERRIDE;
  virtual void ReleaseMouseButtonTitlebar(
      panel::ClickModifier modifier) OVERRIDE;
  virtual void DragTitlebar(const gfx::Point& mouse_location) OVERRIDE;
  virtual void CancelDragTitlebar() OVERRIDE;
  virtual void FinishDragTitlebar() OVERRIDE;
  virtual bool VerifyDrawingAttention() const OVERRIDE;
  virtual bool VerifyActiveState(bool is_active) OVERRIDE;
  virtual void WaitForWindowCreationToComplete() const OVERRIDE;
  virtual bool IsWindowSizeKnown() const OVERRIDE;
  virtual bool IsAnimatingBounds() const OVERRIDE;
  virtual bool IsButtonVisible(TitlebarButtonType button_type) const OVERRIDE;

  PanelBrowserWindowGtk* panel_browser_window_gtk_;
};

// static
NativePanelTesting* NativePanelTesting::Create(NativePanel* native_panel) {
  return new NativePanelTestingGtk(static_cast<PanelBrowserWindowGtk*>(
      native_panel));
}

NativePanelTestingGtk::NativePanelTestingGtk(
    PanelBrowserWindowGtk* panel_browser_window_gtk) :
    panel_browser_window_gtk_(panel_browser_window_gtk) {
}

void NativePanelTestingGtk::PressLeftMouseButtonTitlebar(
    const gfx::Point& mouse_location, panel::ClickModifier modifier) {
  // If there is an animation, wait for it to finish as we don't handle button
  // clicks while animation is in progress.
  while (panel_browser_window_gtk_->IsAnimatingBounds())
    MessageLoopForUI::current()->RunAllPending();

  GdkEvent* event = gdk_event_new(GDK_BUTTON_PRESS);
  event->button.button = 1;
  event->button.x_root = mouse_location.x();
  event->button.y_root = mouse_location.y();
  if (modifier == panel::APPLY_TO_ALL)
    event->button.state |= GDK_CONTROL_MASK;
  panel_browser_window_gtk_->HandleTitleBarLeftMousePress(
      reinterpret_cast<GdkEventButton*>(event),
      GDK_CURRENT_TIME, gfx::Point());
  gdk_event_free(event);
  MessageLoopForUI::current()->RunAllPending();
}

void NativePanelTestingGtk::ReleaseMouseButtonTitlebar(
    panel::ClickModifier modifier) {
  GdkEvent* event = gdk_event_new(GDK_BUTTON_RELEASE);
  event->button.button = 1;
  if (modifier == panel::APPLY_TO_ALL)
    event->button.state |= GDK_CONTROL_MASK;
  if (panel_browser_window_gtk_->drag_helper_.get()) {
    panel_browser_window_gtk_->drag_helper_->OnButtonReleaseEvent(
        NULL, reinterpret_cast<GdkEventButton*>(event));
  } else {
    panel_browser_window_gtk_->OnTitlebarButtonReleaseEvent(
        NULL, reinterpret_cast<GdkEventButton*>(event));
  }
  gdk_event_free(event);
  MessageLoopForUI::current()->RunAllPending();
}

void NativePanelTestingGtk::DragTitlebar(const gfx::Point& mouse_location) {
  if (!panel_browser_window_gtk_->drag_helper_.get())
    return;
  GdkEvent* event = gdk_event_new(GDK_MOTION_NOTIFY);
  event->motion.x_root = mouse_location.x();
  event->motion.y_root = mouse_location.y();
  panel_browser_window_gtk_->drag_helper_->OnMouseMoveEvent(
      NULL, reinterpret_cast<GdkEventMotion*>(event));
  gdk_event_free(event);
  MessageLoopForUI::current()->RunAllPending();
}

void NativePanelTestingGtk::CancelDragTitlebar() {
  if (!panel_browser_window_gtk_->drag_helper_.get())
    return;
  panel_browser_window_gtk_->drag_helper_->OnGrabBrokenEvent(NULL, NULL);
  MessageLoopForUI::current()->RunAllPending();
}

void NativePanelTestingGtk::FinishDragTitlebar() {
  if (!panel_browser_window_gtk_->drag_helper_.get())
    return;
  ReleaseMouseButtonTitlebar(panel::NO_MODIFIER);
}

bool NativePanelTestingGtk::VerifyDrawingAttention() const {
  return panel_browser_window_gtk_->IsDrawingAttention();
}

bool NativePanelTestingGtk::VerifyActiveState(bool is_active) {
  // TODO(jianli): to be implemented. http://crbug.com/102737
  return false;
}

void NativePanelTestingGtk::WaitForWindowCreationToComplete() const {
  while (panel_browser_window_gtk_->frame_size_.IsEmpty())
    MessageLoopForUI::current()->RunAllPending();
  while (panel_browser_window_gtk_->IsAnimatingBounds())
    MessageLoopForUI::current()->RunAllPending();
}

bool NativePanelTestingGtk::IsWindowSizeKnown() const {
  return !panel_browser_window_gtk_->frame_size_.IsEmpty();
}

bool NativePanelTestingGtk::IsAnimatingBounds() const {
  return panel_browser_window_gtk_->IsAnimatingBounds();
}

bool NativePanelTestingGtk::IsButtonVisible(
    TitlebarButtonType button_type) const {
  PanelBrowserTitlebarGtk* titlebar =
      panel_browser_window_gtk_->GetPanelTitlebar();
  CustomDrawButton* button;
  switch (button_type) {
    case CLOSE_BUTTON:
      button = titlebar->close_button();
      break;
    case MINIMIZE_BUTTON:
      button = titlebar->minimize_button();
      break;
    case RESTORE_BUTTON:
      button = titlebar->unminimize_button();
      break;
    default:
      NOTREACHED();
      return false;
  }
  return gtk_widget_get_visible(button->widget());
}
