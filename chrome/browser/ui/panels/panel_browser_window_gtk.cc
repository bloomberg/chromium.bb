// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/panels/panel_browser_window_gtk.h"

#include "base/bind.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/gtk/browser_titlebar.h"
#include "chrome/browser/ui/panels/panel.h"
#include "chrome/browser/ui/panels/panel_bounds_animation.h"
#include "chrome/browser/ui/panels/panel_drag_gtk.h"
#include "chrome/browser/ui/panels/panel_manager.h"
#include "chrome/browser/ui/panels/panel_strip.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/notification_service.h"

using content::WebContents;

namespace {

// RGB values for titlebar in draw attention state. A shade of orange.
const int kDrawAttentionR = 0xfa;
const int kDrawAttentionG = 0x98;
const int kDrawAttentionB = 0x3a;
const float kDrawAttentionRFraction = kDrawAttentionR / 255.0;
const float kDrawAttentionGFraction = kDrawAttentionG / 255.0;
const float kDrawAttentionBFraction = kDrawAttentionB / 255.0;

// Delay before click on a titlebar is allowed to minimize the panel after
// the 'draw attention' mode has been cleared.
const int kSuspendMinimizeOnClickIntervalMs = 500;

// Markup for title text in draw attention state. Set to color white.
const char* const kDrawAttentionTitleMarkupPrefix =
    "<span fgcolor='#ffffff'>";
const char* const kDrawAttentionTitleMarkupSuffix = "</span>";

// Set minimium width for window really small.
const int kMinWindowWidth = 26;

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
      chrome::NOTIFICATION_PANEL_CHANGED_LAYOUT_MODE,
      content::Source<Panel>(panel_.get()));
  registrar_.Add(
      this,
      chrome::NOTIFICATION_WINDOW_CLOSED,
      content::Source<GtkWindow>(window()));
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

bool PanelBrowserWindowGtk::UseCustomFrame() {
  // We always use custom frame for panels.
  return TRUE;
}

void PanelBrowserWindowGtk::DrawPopupFrame(cairo_t* cr,
                                           GtkWidget* widget,
                                           GdkEventExpose* event) {
  BrowserWindowGtk::DrawPopupFrame(cr, widget, event);

  if (is_drawing_attention_)
    DrawAttentionFrame(cr, widget, event);
}

void PanelBrowserWindowGtk::DrawCustomFrame(cairo_t* cr,
                                            GtkWidget* widget,
                                            GdkEventExpose* event) {
  BrowserWindowGtk::DrawCustomFrame(cr, widget, event);

  if (is_drawing_attention_)
    DrawAttentionFrame(cr, widget, event);
}

void PanelBrowserWindowGtk::DrawAttentionFrame(cairo_t* cr,
                                               GtkWidget* widget,
                                               GdkEventExpose* event) {
  cairo_set_source_rgb(cr, kDrawAttentionRFraction,
                       kDrawAttentionGFraction,
                       kDrawAttentionBFraction);

  GdkRectangle dest_rectangle = GetTitlebarRectForDrawAttention();
  GdkRegion* dest_region = gdk_region_rectangle(&dest_rectangle);

  gdk_region_intersect(dest_region, event->region);
  gdk_cairo_region(cr, dest_region);

  cairo_clip(cr);
  cairo_paint(cr);
  gdk_region_destroy(dest_region);
}

void PanelBrowserWindowGtk::ActiveWindowChanged(GdkWindow* active_window) {
  bool was_active = IsActive();
  BrowserWindowGtk::ActiveWindowChanged(active_window);
  if (!window() || was_active == IsActive())  // State didn't change.
    return;

  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_PANEL_CHANGED_ACTIVE_STATUS,
      content::Source<Panel>(panel_.get()),
      content::NotificationService::NoDetails());
  panel_->OnActiveStateChanged();
}

BrowserWindowGtk::TitleDecoration PanelBrowserWindowGtk::GetWindowTitle(
    std::string* title) const {
  if (is_drawing_attention_) {
    std::string title_original;
    BrowserWindowGtk::TitleDecoration title_decoration =
        BrowserWindowGtk::GetWindowTitle(&title_original);
    DCHECK_EQ(BrowserWindowGtk::PLAIN_TEXT, title_decoration);
    gchar* title_escaped = g_markup_escape_text(title_original.c_str(), -1);
    gchar* title_with_markup = g_strconcat(kDrawAttentionTitleMarkupPrefix,
                                           title_escaped,
                                           kDrawAttentionTitleMarkupSuffix,
                                           NULL);
    *title = title_with_markup;
    g_free(title_escaped);
    g_free(title_with_markup);
    return BrowserWindowGtk::PANGO_MARKUP;
  } else {
    return BrowserWindowGtk::GetWindowTitle(title);
  }
}

void PanelBrowserWindowGtk::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_PANEL_CHANGED_LAYOUT_MODE:
      titlebar()->UpdateCustomFrame(true);
      break;
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

  if (!bounds_.width() || !bounds_.height()) {
    // If the old bounds are 0 in either dimension, we need to show the
    // window as it would now be hidden.
    gtk_widget_show(GTK_WIDGET(window()));
    gdk_window_move(gtk_widget_get_window(GTK_WIDGET(window())), bounds_.x(),
                    bounds_.y());
  }

  if (!animate) {
    // If no animation is in progress, apply bounds change instantly. Otherwise,
    // continue the animation with new target bounds.
    if (!IsAnimatingBounds()) {
      if (bounds_.origin() != bounds.origin())
        gtk_window_move(window(), bounds.x(), bounds.y());
      if (bounds_.size() != bounds.size())
        ResizeWindow(bounds.width(), bounds.height());
    }
  } else if (!frame_size_.IsEmpty()) {
    StartBoundsAnimation(bounds_, bounds);
  } else {
    // Wait until the window frame has been sized before starting the animation
    // to avoid animating the window before it is shown.
  }

  bounds_ = bounds;
}

void PanelBrowserWindowGtk::ResizeWindow(int width, int height) {
  // Gtk does not allow window size to be set to 0, so make it 1 if it's 0.
  gtk_window_resize(window(), std::max(width, 1), std::max(height, 1));
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

  GdkRectangle rect = GetTitlebarRectForDrawAttention();
  gdk_window_invalidate_rect(
      gtk_widget_get_window(GTK_WIDGET(window())), &rect, TRUE);

  UpdateTitleBar();

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

gfx::Size PanelBrowserWindowGtk::IconOnlySize() const {
  GtkAllocation allocation;
  gtk_widget_get_allocation(titlebar_widget(), &allocation);
  return gfx::Size(titlebar()->IconOnlyWidth(), allocation.height);
}

void PanelBrowserWindowGtk::EnsurePanelFullyVisible() {
  gtk_window_present(window());
}

void PanelBrowserWindowGtk::SetPanelAppIconVisibility(bool visible) {
  return;
}

void PanelBrowserWindowGtk::SetPanelAlwaysOnTop(bool on_top) {
  gtk_window_set_keep_above(window(), on_top);
}

void PanelBrowserWindowGtk::EnableResizeByMouse(bool enable) {
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
  titlebar()->SendEnterNotifyToCloseButtonIfUnderMouse();

  // If the final size is 0 in either dimension, hide the window as gtk does
  // not allow the window size to be 0.
  if (!bounds_.width() || !bounds_.height())
    gtk_widget_hide(GTK_WIDGET(window()));

  panel_->manager()->OnPanelAnimationEnded(panel_.get());
}

void PanelBrowserWindowGtk::AnimationProgressed(
    const ui::Animation* animation) {
  DCHECK(!frame_size_.IsEmpty());

  gfx::Rect new_bounds = bounds_animator_->CurrentValueBetween(
      animation_start_bounds_, bounds_);

  gdk_window_move_resize(gtk_widget_get_window(GTK_WIDGET(window())),
                         new_bounds.x(), new_bounds.y(),
                         std::max(new_bounds.width(), 1),  // 0 not allowed
                         std::max(new_bounds.height(), 1));  // 0 not allowed

  last_animation_progressed_bounds_ = new_bounds;
}

GdkRectangle PanelBrowserWindowGtk::GetTitlebarRectForDrawAttention() const {
  GdkRectangle rect;
  rect.x = 0;
  rect.y = 0;
  // We get the window width and not the titlebar_widget() width because we'd
  // like for the window borders on either side of the title bar to be the same
  // color.
  GtkAllocation window_allocation;
  gtk_widget_get_allocation(GTK_WIDGET(window()), &window_allocation);
  rect.width = window_allocation.width;

  GtkAllocation titlebar_allocation;
  gtk_widget_get_allocation(titlebar_widget(), &titlebar_allocation);
  rect.height = titlebar_allocation.height;

  return rect;
}

gboolean PanelBrowserWindowGtk::OnTitlebarButtonReleaseEvent(
    GtkWidget* widget, GdkEventButton* event) {
  if (event->button != 1)
    return TRUE;

  if (event->state & GDK_CONTROL_MASK) {
    panel_->OnTitlebarClicked(panel::APPLY_TO_ALL);
    return TRUE;
  }

  // TODO(jennb): Move remaining titlebar click handling out of here.
  // (http://crbug.com/118431)
  PanelStrip* panel_strip = panel_->panel_strip();
  if (!panel_strip)
    return TRUE;

  if (panel_strip->type() == PanelStrip::DOCKED &&
      panel_->expansion_state() == Panel::EXPANDED) {
    if (base::Time::Now() < disableMinimizeUntilTime_)
      return TRUE;

    panel_->SetExpansionState(Panel::MINIMIZED);
  } else {
    panel_->Activate();
  }

  return TRUE;
}

void PanelBrowserWindowGtk::HandleFocusIn(GtkWidget* widget,
                                          GdkEventFocus* event) {
  BrowserWindowGtk::HandleFocusIn(widget, event);

  // Do not clear draw attention if user cannot see contents of panel.
  if (!is_drawing_attention_ || panel_->IsMinimized())
    return;

  panel_->FlashFrame(false);

  disableMinimizeUntilTime_ = base::Time::Now() +
      base::TimeDelta::FromMilliseconds(kSuspendMinimizeOnClickIntervalMs);
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

  panel_browser_window_gtk_->drag_helper_->EndDrag(true);
}

void NativePanelTestingGtk::FinishDragTitlebar() {
  if (!panel_browser_window_gtk_->drag_helper_.get())
    return;

  panel_browser_window_gtk_->drag_helper_->EndDrag(false);
}

bool NativePanelTestingGtk::VerifyDrawingAttention() const {
  std::string title;
  BrowserWindowGtk::TitleDecoration decoration =
      panel_browser_window_gtk_->GetWindowTitle(&title);
  return panel_browser_window_gtk_->IsDrawingAttention() &&
         decoration == BrowserWindowGtk::PANGO_MARKUP;
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
