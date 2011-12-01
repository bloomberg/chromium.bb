// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/panels/panel_browser_window_gtk.h"

#include "base/bind.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/panels/panel.h"
#include "chrome/browser/ui/panels/panel_manager.h"
#include "chrome/browser/ui/panels/panel_settings_menu_model.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/notification_service.h"
#include "ui/base/animation/slide_animation.h"
#include "ui/base/dragdrop/gtk_dnd_util.h"
#include "ui/base/x/work_area_watcher_x.h"

namespace {

// This value is experimental and subjective.
const int kSetBoundsAnimationMs = 180;

// RGB values for titlebar in draw attention state. A shade of orange.
const int kDrawAttentionR = 0xfa;
const int kDrawAttentionG = 0x98;
const int kDrawAttentionB = 0x3a;
const float kDrawAttentionRFraction = kDrawAttentionR / 255.0;
const float kDrawAttentionGFraction = kDrawAttentionG / 255.0;
const float kDrawAttentionBFraction = kDrawAttentionB / 255.0;

// Delay before click on a titlebar is allowed to minimize the panel after
// the 'draw attention' mode has been cleared.
const base::TimeDelta kSuspendMinimizeOnClickIntervalMs =
    base::TimeDelta::FromMilliseconds(500);

// Markup for title text in draw attention state. Set to color white.
const char* const kDrawAttentionTitleMarkupPrefix =
    "<span fgcolor='#ffffff'>";
const char* const kDrawAttentionTitleMarkupSuffix = "</span>";

}

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
      last_mouse_down_(NULL),
      drag_widget_(NULL),
      destroy_drag_widget_factory_(this),
      drag_end_factory_(this),
      panel_(panel),
      bounds_(bounds),
      window_size_known_(false),
      is_drawing_attention_(false) {
}

PanelBrowserWindowGtk::~PanelBrowserWindowGtk() {
  if (drag_widget_) {
    // Terminate the grab if we have it. We could do this using any widget,
    // |drag_widget_| is just convenient.
    gtk_grab_add(drag_widget_);
    gtk_grab_remove(drag_widget_);
    DestroyDragWidget();
  }
  panel_->OnNativePanelClosed();
  ui::WorkAreaWatcherX::RemoveObserver(this);
}

void PanelBrowserWindowGtk::Init() {
  BrowserWindowGtk::Init();

  // Keep the window docked to the bottom of the screen on resizes.
  gtk_window_set_gravity(window(), GDK_GRAVITY_SOUTH_EAST);

  // Keep the window always on top.
  gtk_window_set_keep_above(window(), TRUE);

  // Show the window on all the virtual desktops.
  gtk_window_stick(window());

  // Do not show an icon in the task bar.  Window operations such as close,
  // minimize etc. can only be done from the panel UI.
  gtk_window_set_skip_taskbar_hint(window(), TRUE);

  g_signal_connect(titlebar_widget(), "button-press-event",
                   G_CALLBACK(OnTitlebarButtonPressEventThunk), this);
  g_signal_connect(titlebar_widget(), "button-release-event",
                   G_CALLBACK(OnTitlebarButtonReleaseEventThunk), this);

  ui::WorkAreaWatcherX::AddObserver(this);
}

bool PanelBrowserWindowGtk::GetWindowEdge(int x, int y, GdkWindowEdge* edge) {
  // Since panels are not resizable or movable by the user, we should not
  // detect the window edge for behavioral purposes.  The edge, if any,
  // is present only for visual aspects.
  return FALSE;
}

bool PanelBrowserWindowGtk::HandleTitleBarLeftMousePress(
    GdkEventButton* event,
    guint32 last_click_time,
    gfx::Point last_click_position) {
  // In theory we should never enter this function as we have a handler for
  // button press on titlebar where we handle this.  Not putting NOTREACHED()
  // here because we don't want to crash if hit-testing for titlebar in window
  // button press handler in BrowserWindowGtk is off by a pixel or two.
  DLOG(WARNING) << "Hit-testing for titlebar off by a pixel or two?";
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
  hints.min_width = panel_->min_size().width();
  gtk_window_set_geometry_hints(
      window(), GTK_WIDGET(window()), &hints, GDK_HINT_MIN_SIZE);

  DCHECK(!window_size_known_);
}

void PanelBrowserWindowGtk::SetBounds(const gfx::Rect& bounds) {
  // This should never be called.
  DLOG(WARNING) << "Unexpected call to PanelBrowserWindowGtk::SetBounds()";
}

void PanelBrowserWindowGtk::OnSizeChanged(int width, int height) {
  BrowserWindowGtk::OnSizeChanged(width, height);

  if (window_size_known_)
    return;

  window_size_known_ = true;
  int top = bounds_.bottom() - height;
  int left = bounds_.right() - width;

  gtk_window_move(window_, left, top);
  StartBoundsAnimation(gfx::Rect(left, top, width, height));
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

void PanelBrowserWindowGtk::ShowSettingsMenu(GtkWidget* widget,
                                             GdkEventButton* event) {
  if (!settings_menu_.get()) {
    settings_menu_model_.reset(new PanelSettingsMenuModel(panel_.get()));
    settings_menu_.reset(new MenuGtk(this, settings_menu_model_.get()));
  }
  settings_menu_->PopupForWidget(widget, event->button, event->time);
}

void PanelBrowserWindowGtk::DrawCustomFrame(cairo_t* cr,
                                            GtkWidget* widget,
                                            GdkEventExpose* event) {
  BrowserWindowGtk::DrawCustomFrame(cr, widget, event);

  if (is_drawing_attention_) {
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
}

void PanelBrowserWindowGtk::ActiveWindowChanged(GdkWindow* active_window) {
  bool was_active = IsActive();
  BrowserWindowGtk::ActiveWindowChanged(active_window);
  if (was_active == IsActive())  // State didn't change.
    return;

  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_PANEL_CHANGED_ACTIVE_STATUS,
      content::Source<Panel>(panel_.get()),
      content::NotificationService::NoDetails());
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

void PanelBrowserWindowGtk::WorkAreaChanged() {
  panel_->manager()->OnDisplayChanged();
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
  if (bounds == bounds_)
    return;

  if (drag_widget_) {
    DCHECK(!bounds_animator_.get() || !bounds_animator_->is_animating());
    // If the current panel is being dragged, it should just move with the
    // user drag, we should not animate.
    gtk_window_move(window(), bounds.x(), bounds.y());
  } else if (window_size_known_) {
    StartBoundsAnimation(bounds_);
  }
  // If window size is not known, wait till the size is known before starting
  // the animation.

  bounds_ = bounds;
}

void PanelBrowserWindowGtk::ClosePanel() {
  // Cancel any currently running animation since we're closing down.
  if (bounds_animator_.get())
    bounds_animator_.reset();

  Close();
}

void PanelBrowserWindowGtk::ActivatePanel() {
  gdk_window_set_accept_focus(GTK_WIDGET(window())->window, TRUE);
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

  if (panel_->expansion_state() == Panel::MINIMIZED)
    gdk_window_set_accept_focus(GTK_WIDGET(window())->window, FALSE);
}

bool PanelBrowserWindowGtk::IsPanelActive() const {
  return IsActive();
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

void PanelBrowserWindowGtk::PanelTabContentsFocused(TabContents* tab_contents) {
  TabContentsFocused(tab_contents);
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

void PanelBrowserWindowGtk::DrawAttention() {
  // Don't draw attention for active panel.
  if (is_drawing_attention_ || IsActive())
    return;

  is_drawing_attention_ = true;
  // Bring up the titlebar to get people's attention.
  if (panel_->expansion_state() == Panel::MINIMIZED)
    panel_->SetExpansionState(Panel::TITLE_ONLY);

  GdkRectangle rect = GetTitlebarRectForDrawAttention();
  gdk_window_invalidate_rect(GTK_WIDGET(window())->window, &rect, TRUE);

  UpdateTitleBar();
}

bool PanelBrowserWindowGtk::IsDrawingAttention() const {
  return is_drawing_attention_;
}

bool PanelBrowserWindowGtk::PreHandlePanelKeyboardEvent(
    const NativeWebKeyboardEvent& event,
    bool* is_keyboard_shortcut) {
  return PreHandleKeyboardEvent(event, is_keyboard_shortcut);
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
  // TODO(prasdt): to be implemented.
  return gfx::Size();
}

void PanelBrowserWindowGtk::EnsurePanelFullyVisible() {
  // TODO(prasdt): to be implemented.
}

gfx::Size PanelBrowserWindowGtk::WindowSizeFromContentSize(
    const gfx::Size& content_size) const {
  gfx::Size frame = GetNonClientFrameSize();
  return gfx::Size(content_size.width() + frame.width(),
                   content_size.height() + frame.height());
}

gfx::Size PanelBrowserWindowGtk::ContentSizeFromWindowSize(
    const gfx::Size& window_size) const {
  gfx::Size frame = GetNonClientFrameSize();
  return gfx::Size(window_size.width() - frame.width(),
                   window_size.height() - frame.height());
}

int PanelBrowserWindowGtk::TitleOnlyHeight() const {
  return titlebar_widget()->allocation.height;
}

void PanelBrowserWindowGtk::StartBoundsAnimation(
    const gfx::Rect& current_bounds) {
  animation_start_bounds_ = current_bounds;

  if (!bounds_animator_.get()) {
    bounds_animator_.reset(new ui::SlideAnimation(this));
    bounds_animator_->SetSlideDuration(kSetBoundsAnimationMs);
  }

  if (bounds_animator_->IsShowing())
    bounds_animator_->Reset();
  bounds_animator_->Show();
}

bool PanelBrowserWindowGtk::IsAnimatingBounds() const {
  return bounds_animator_.get() && bounds_animator_->is_animating();
}

void PanelBrowserWindowGtk::WillProcessEvent(GdkEvent* event) {
  // Nothing to do.
}

void PanelBrowserWindowGtk::DidProcessEvent(GdkEvent* event) {
  DCHECK(last_mouse_down_);
  if (event->type != GDK_MOTION_NOTIFY)
    return;

  gdouble new_x_double;
  gdouble new_y_double;
  gdouble old_x_double;
  gdouble old_y_double;
  gdk_event_get_root_coords(event, &new_x_double, &new_y_double);
  gdk_event_get_root_coords(last_mouse_down_, &old_x_double, &old_y_double);

  gint new_x = static_cast<gint>(new_x_double);
  gint new_y = static_cast<gint>(new_y_double);
  gint old_x = static_cast<gint>(old_x_double);
  gint old_y = static_cast<gint>(old_y_double);

  if (drag_widget_) {
    panel_->manager()->Drag(new_x - old_x);
    gdk_event_free(last_mouse_down_);
    last_mouse_down_ = gdk_event_copy(event);
  } else if (gtk_drag_check_threshold(titlebar_widget(), old_x,
                                      old_y, new_x, new_y)) {
    CreateDragWidget();
    GtkTargetList* list = ui::GetTargetListFromCodeMask(ui::CHROME_TAB);
    gtk_drag_begin(drag_widget_, list, GDK_ACTION_MOVE, 1, last_mouse_down_);
    // gtk_drag_begin increments reference count for GtkTargetList.  So unref
    // it here to reduce the reference count.
    gtk_target_list_unref(list);
    panel_->manager()->StartDragging(panel_.get());
  }
}

void PanelBrowserWindowGtk::AnimationEnded(const ui::Animation* animation) {
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_PANEL_BOUNDS_ANIMATIONS_FINISHED,
      content::Source<Panel>(panel_.get()),
      content::NotificationService::NoDetails());
}

void PanelBrowserWindowGtk::AnimationProgressed(
    const ui::Animation* animation) {
  DCHECK(!drag_widget_);
  DCHECK(window_size_known_);

  gfx::Rect new_bounds = bounds_animator_->CurrentValueBetween(
      animation_start_bounds_, bounds_);

  // Resize if necessary.
  if (animation_start_bounds_.size() != bounds_.size())
    gtk_window_resize(window(), new_bounds.width(), new_bounds.height());

  // Only move if bottom right corner will change.
  // Panels use window gravity of GDK_GRAVITY_SOUTH_EAST which means the
  // window is anchored to the bottom right corner on resize, making it
  // unnecessary to move the window if the bottom right corner is unchanged.
  // For example, when we minimize to the bottom, moving can actually
  // result in the wrong behavior.
  //   - Say window is 100x100 with x,y=900,900 on a 1000x1000 screen.
  //   - Say you minimize the window to 100x3 and move it to 900,997 to keep it
  //     anchored to the bottom.
  //   - resize is an async operation and the window manager will decide that
  //     the move will take the window off screen and it won't honor the
  //     request.
  //   - When resize finally happens, you'll have a 100x3 window a x,y=900,900.
  bool move = (animation_start_bounds_.bottom() != bounds_.bottom()) ||
              (animation_start_bounds_.right() != bounds_.right());
  if (move)
    gtk_window_move(window_, new_bounds.x(), new_bounds.y());
}

void PanelBrowserWindowGtk::CreateDragWidget() {
  DCHECK(!drag_widget_);
  drag_widget_ = gtk_invisible_new();
  g_signal_connect_after(drag_widget_, "drag-begin",
                         G_CALLBACK(OnDragBeginThunk), this);
  g_signal_connect(drag_widget_, "drag-failed",
                   G_CALLBACK(OnDragFailedThunk), this);
  g_signal_connect(drag_widget_, "button-release-event",
                   G_CALLBACK(OnDragButtonReleasedThunk), this);
}

void PanelBrowserWindowGtk::DestroyDragWidget() {
  if (drag_widget_) {
    gtk_widget_destroy(drag_widget_);
    drag_widget_ = NULL;
  }
}

void PanelBrowserWindowGtk::EndDrag(bool canceled) {
  // Make sure we only run EndDrag once by canceling any tasks that want
  // to call EndDrag.
  drag_end_factory_.InvalidateWeakPtrs();

  // We must let gtk clean up after we handle the drag operation, otherwise
  // there will be outstanding references to the drag widget when we try to
  // destroy it.
  MessageLoop::current()->PostTask(FROM_HERE,
      base::Bind(&PanelBrowserWindowGtk::DestroyDragWidget,
                 drag_end_factory_.GetWeakPtr()));
  CleanupDragDrop();
  panel_->manager()->EndDragging(canceled);
}

void PanelBrowserWindowGtk::CleanupDragDrop() {
  if (last_mouse_down_) {
    MessageLoopForUI::current()->RemoveObserver(this);
    gdk_event_free(last_mouse_down_);
    last_mouse_down_ = NULL;
  }
}

GdkRectangle PanelBrowserWindowGtk::GetTitlebarRectForDrawAttention() const {
  GdkRectangle rect;
  rect.x = 0;
  rect.y = 0;
  // We get the window width and not the titlebar_widget() width because we'd
  // like for the window borders on either side of the title bar to be the same
  // color.
  rect.width = GTK_WIDGET(window())->allocation.width;
  rect.height = titlebar_widget()->allocation.height;

  return rect;
}

gboolean PanelBrowserWindowGtk::OnTitlebarButtonPressEvent(
    GtkWidget* widget, GdkEventButton* event) {
  // Early return if animation in progress.
  if (IsAnimatingBounds())
    return TRUE;

  // Every button press ensures either a button-release-event or a drag-fail
  // signal for |widget|.
  if (event->button == 1 && event->type == GDK_BUTTON_PRESS) {
    // Store the button press event, used to initiate a drag.
    DCHECK(!last_mouse_down_);
    last_mouse_down_ = gdk_event_copy(reinterpret_cast<GdkEvent*>(event));
    MessageLoopForUI::current()->AddObserver(this);
  }

  return TRUE;
}

gboolean PanelBrowserWindowGtk::OnTitlebarButtonReleaseEvent(
    GtkWidget* widget, GdkEventButton* event) {
  if (event->button != 1) {
    DCHECK(!last_mouse_down_);
    return TRUE;
  }

  CleanupDragDrop();

  if (panel_->expansion_state() == Panel::EXPANDED) {
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

  if (!is_drawing_attention_)
    return;

  is_drawing_attention_ = false;
  UpdateTitleBar();
  DCHECK(panel_->expansion_state() == Panel::EXPANDED);

  disableMinimizeUntilTime_ =
      base::Time::Now() + kSuspendMinimizeOnClickIntervalMs;
}

void PanelBrowserWindowGtk::OnDragBegin(GtkWidget* widget,
                                        GdkDragContext* context) {
  // Set drag icon to be a transparent pixbuf.
  GdkPixbuf* pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, TRUE, 8, 1, 1);
  gdk_pixbuf_fill(pixbuf, 0);
  gtk_drag_set_icon_pixbuf(context, pixbuf, 0, 0);
  g_object_unref(pixbuf);
}

gboolean PanelBrowserWindowGtk::OnDragFailed(
    GtkWidget* widget, GdkDragContext* context, GtkDragResult result) {
  bool canceled = (result != GTK_DRAG_RESULT_NO_TARGET);
  EndDrag(canceled);
  return TRUE;
}

gboolean PanelBrowserWindowGtk::OnDragButtonReleased(GtkWidget* widget,
                                                     GdkEventButton* button) {
  // We always get this event when gtk is releasing the grab and ending the
  // drag.  This gets fired before drag-failed handler gets fired.  However,
  // if the user ended the drag with space or enter, we don't get a follow up
  // event to tell us the drag has finished (either a drag-failed or a
  // drag-end).  We post a task, instead of calling EndDrag right here, to give
  // GTK+ a chance to send the drag-failed event with the right status.  If
  // GTK+ does send the drag-failed event, we cancel the task.
  MessageLoop::current()->PostTask(FROM_HERE,
      base::Bind(&PanelBrowserWindowGtk::EndDrag,
                 drag_end_factory_.GetWeakPtr(),
                 false));
  return TRUE;
}

// NativePanelTesting implementation.
class NativePanelTestingGtk : public NativePanelTesting {
 public:
  explicit NativePanelTestingGtk(
      PanelBrowserWindowGtk* panel_browser_window_gtk);

 private:
  virtual void PressLeftMouseButtonTitlebar(
      const gfx::Point& point) OVERRIDE;
  virtual void ReleaseMouseButtonTitlebar() OVERRIDE;
  virtual void DragTitlebar(int delta_x, int delta_y) OVERRIDE;
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
    const gfx::Point& point) {
  // If there is an animation, wait for it to finish as we don't handle button
  // clicks while animation is in progress.
  while (panel_browser_window_gtk_->IsAnimatingBounds())
    MessageLoopForUI::current()->RunAllPending();

  GdkEvent* event = gdk_event_new(GDK_BUTTON_PRESS);
  event->button.button = 1;
  event->button.x_root = point.x();
  event->button.y_root = point.y();
  panel_browser_window_gtk_->OnTitlebarButtonPressEvent(
      panel_browser_window_gtk_->titlebar_widget(),
      reinterpret_cast<GdkEventButton*>(event));
  gdk_event_free(event);
  MessageLoopForUI::current()->RunAllPending();
}

void NativePanelTestingGtk::ReleaseMouseButtonTitlebar() {
  GdkEvent* event = gdk_event_new(GDK_BUTTON_PRESS);
  event->button.button = 1;
  panel_browser_window_gtk_->OnTitlebarButtonReleaseEvent(
      panel_browser_window_gtk_->titlebar_widget(),
      reinterpret_cast<GdkEventButton*>(event));
  MessageLoopForUI::current()->RunAllPending();
}

void NativePanelTestingGtk::DragTitlebar(int delta_x, int delta_y) {
  if (!panel_browser_window_gtk_->drag_widget_) {
    panel_browser_window_gtk_->CreateDragWidget();
    panel_browser_window_gtk_->panel_->manager()->StartDragging(
        panel_browser_window_gtk_->panel_.get());
  }
  GdkEvent* event = gdk_event_new(GDK_MOTION_NOTIFY);
  gdk_event_get_root_coords(panel_browser_window_gtk_->last_mouse_down_,
      &event->motion.x_root, &event->motion.y_root);
  event->motion.x_root += delta_x;
  event->motion.y_root += delta_y;
  panel_browser_window_gtk_->DidProcessEvent(event);
  gdk_event_free(event);
  MessageLoopForUI::current()->RunAllPending();
}

void NativePanelTestingGtk::CancelDragTitlebar() {
  panel_browser_window_gtk_->OnDragFailed(
      panel_browser_window_gtk_->drag_widget_, NULL,
      GTK_DRAG_RESULT_USER_CANCELLED);
  MessageLoopForUI::current()->RunAllPending();
}

void NativePanelTestingGtk::FinishDragTitlebar() {
  panel_browser_window_gtk_->OnDragFailed(
      panel_browser_window_gtk_->drag_widget_, NULL,
      GTK_DRAG_RESULT_NO_TARGET);
  MessageLoopForUI::current()->RunAllPending();
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
  while (!panel_browser_window_gtk_->window_size_known_)
    MessageLoopForUI::current()->RunAllPending();
  while (panel_browser_window_gtk_->bounds_animator_.get() &&
         panel_browser_window_gtk_->bounds_animator_->is_animating()) {
    MessageLoopForUI::current()->RunAllPending();
  }
}

bool NativePanelTestingGtk::IsWindowSizeKnown() const {
  return panel_browser_window_gtk_->window_size_known_;
}

bool NativePanelTestingGtk::IsAnimatingBounds() const {
  return panel_browser_window_gtk_->IsAnimatingBounds();
}
