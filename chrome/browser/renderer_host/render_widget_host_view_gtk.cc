// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/render_widget_host_view_gtk.h"

// If this gets included after the gtk headers, then a bunch of compiler
// errors happen because of a "#define Status int" in Xlib.h, which interacts
// badly with URLRequestStatus::Status.
#include "chrome/common/render_messages.h"

#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gdk/gdkkeysyms.h>
#include <gdk/gdkx.h>
#include <cairo/cairo.h>

#include <algorithm>
#include <string>

#include "app/gfx/gtk_util.h"
#include "app/l10n_util.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/string_util.h"
#include "base/task.h"
#include "base/time.h"
#include "chrome/browser/renderer_host/backing_store_x.h"
#include "chrome/browser/renderer_host/gpu_view_host.h"
#include "chrome/browser/renderer_host/gtk_im_context_wrapper.h"
#include "chrome/browser/renderer_host/gtk_key_bindings_handler.h"
#include "chrome/browser/renderer_host/render_widget_host.h"
#include "chrome/common/gtk_util.h"
#include "chrome/common/native_web_keyboard_event.h"
#include "chrome/common/x11_util.h"
#include "third_party/WebKit/WebKit/chromium/public/gtk/WebInputEventFactory.h"
#include "webkit/glue/webcursor_gtk_data.h"

static const int kMaxWindowWidth = 4000;
static const int kMaxWindowHeight = 4000;

using WebKit::WebInputEventFactory;

// This class is a simple convenience wrapper for Gtk functions. It has only
// static methods.
class RenderWidgetHostViewGtkWidget {
 public:
  static GtkWidget* CreateNewWidget(RenderWidgetHostViewGtk* host_view) {
    GtkWidget* widget = gtk_fixed_new();
    gtk_widget_set_name(widget, "chrome-render-widget-host-view");
    gtk_fixed_set_has_window(GTK_FIXED(widget), TRUE);
    gtk_widget_set_double_buffered(widget, FALSE);
    gtk_widget_set_redraw_on_allocate(widget, FALSE);
#if defined(NDEBUG)
    gtk_widget_modify_bg(widget, GTK_STATE_NORMAL, &gfx::kGdkWhite);
#else
    gtk_widget_modify_bg(widget, GTK_STATE_NORMAL, &gfx::kGdkGreen);
#endif
    // Allow the browser window to be resized freely.
    gtk_widget_set_size_request(widget, 0, 0);

    gtk_widget_add_events(widget, GDK_EXPOSURE_MASK |
                                  GDK_POINTER_MOTION_MASK |
                                  GDK_BUTTON_PRESS_MASK |
                                  GDK_BUTTON_RELEASE_MASK |
                                  GDK_KEY_PRESS_MASK |
                                  GDK_KEY_RELEASE_MASK |
                                  GDK_FOCUS_CHANGE_MASK |
                                  GDK_ENTER_NOTIFY_MASK |
                                  GDK_LEAVE_NOTIFY_MASK);
    GTK_WIDGET_SET_FLAGS(widget, GTK_CAN_FOCUS);

    g_signal_connect(widget, "expose-event",
                     G_CALLBACK(ExposeEvent), host_view);
    g_signal_connect(widget, "key-press-event",
                     G_CALLBACK(KeyPressReleaseEvent), host_view);
    g_signal_connect(widget, "key-release-event",
                     G_CALLBACK(KeyPressReleaseEvent), host_view);
    g_signal_connect(widget, "focus-in-event",
                     G_CALLBACK(OnFocusIn), host_view);
    g_signal_connect(widget, "focus-out-event",
                     G_CALLBACK(OnFocusOut), host_view);
    g_signal_connect(widget, "grab-notify",
                     G_CALLBACK(OnGrabNotify), host_view);
    g_signal_connect(widget, "button-press-event",
                     G_CALLBACK(ButtonPressReleaseEvent), host_view);
    g_signal_connect(widget, "button-release-event",
                     G_CALLBACK(ButtonPressReleaseEvent), host_view);
    g_signal_connect(widget, "motion-notify-event",
                     G_CALLBACK(MouseMoveEvent), host_view);
    g_signal_connect(widget, "enter-notify-event",
                     G_CALLBACK(CrossingEvent), host_view);
    g_signal_connect(widget, "leave-notify-event",
                     G_CALLBACK(CrossingEvent), host_view);

    // Connect after so that we are called after the handler installed by the
    // TabContentsView which handles zoom events.
    g_signal_connect_after(widget, "scroll-event",
                           G_CALLBACK(MouseScrollEvent), host_view);

    return widget;
  }

 private:
  static gboolean ExposeEvent(GtkWidget* widget, GdkEventExpose* expose,
                              RenderWidgetHostViewGtk* host_view) {
    const gfx::Rect damage_rect(expose->area);
    host_view->Paint(damage_rect);
    return FALSE;
  }

  static gboolean KeyPressReleaseEvent(GtkWidget* widget, GdkEventKey* event,
                                       RenderWidgetHostViewGtk* host_view) {
    if (host_view->parent_ && host_view->activatable() &&
        GDK_Escape == event->keyval) {
      // Force popups to close on Esc just in case the renderer is hung.  This
      // allows us to release our keyboard grab.
      host_view->host_->Shutdown();
    } else {
      // Send key event to input method.
      host_view->im_context_->ProcessKeyEvent(event);
    }

    // We return TRUE because we did handle the event. If it turns out webkit
    // can't handle the event, we'll deal with it in
    // RenderView::UnhandledKeyboardEvent().
    return TRUE;
  }

  // WARNING: OnGrabNotify relies on the fact this function doesn't try to
  // dereference |focus|.
  static gboolean OnFocusIn(GtkWidget* widget, GdkEventFocus* focus,
                            RenderWidgetHostViewGtk* host_view) {
    int x, y;
    gtk_widget_get_pointer(widget, &x, &y);
    // http://crbug.com/13389
    // If the cursor is in the render view, fake a mouse move event so that
    // webkit updates its state. Otherwise webkit might think the cursor is
    // somewhere it's not.
    if (x >= 0 && y >= 0 && x < widget->allocation.width &&
        y < widget->allocation.height) {
      WebKit::WebMouseEvent fake_event;
      fake_event.timeStampSeconds = base::Time::Now().ToDoubleT();
      fake_event.modifiers = 0;
      fake_event.windowX = fake_event.x = x;
      fake_event.windowY = fake_event.y = y;
      gdk_window_get_origin(widget->window, &x, &y);
      fake_event.globalX = fake_event.x + x;
      fake_event.globalY = fake_event.y + y;
      fake_event.type = WebKit::WebInputEvent::MouseMove;
      fake_event.button = WebKit::WebMouseEvent::ButtonNone;
      host_view->GetRenderWidgetHost()->ForwardMouseEvent(fake_event);
    }

    host_view->ShowCurrentCursor();
    host_view->GetRenderWidgetHost()->GotFocus();

    // The only way to enable a GtkIMContext object is to call its focus in
    // handler.
    host_view->im_context_->OnFocusIn();

    return FALSE;
  }

  // WARNING: OnGrabNotify relies on the fact this function doesn't try to
  // dereference |focus|.
  static gboolean OnFocusOut(GtkWidget* widget, GdkEventFocus* focus,
                             RenderWidgetHostViewGtk* host_view) {
    // Whenever we lose focus, set the cursor back to that of our parent window,
    // which should be the default arrow.
    gdk_window_set_cursor(widget->window, NULL);
    // If we are showing a context menu, maintain the illusion that webkit has
    // focus.
    if (!host_view->is_showing_context_menu_)
      host_view->GetRenderWidgetHost()->Blur();

    // Disable the GtkIMContext object.
    host_view->im_context_->OnFocusOut();

    return FALSE;
  }

  // Called when we are shadowed or unshadowed by a keyboard grab (which will
  // occur for activatable popups, such as dropdown menus). Popup windows do not
  // take focus, so we never get a focus out or focus in event when they are
  // shown, and must rely on this signal instead.
  static void OnGrabNotify(GtkWidget* widget, gboolean was_grabbed,
                           RenderWidgetHostViewGtk* host_view) {
    if (was_grabbed) {
      if (host_view->was_focused_before_grab_)
        OnFocusIn(widget, NULL, host_view);
    } else {
      host_view->was_focused_before_grab_ = host_view->HasFocus();
      if (host_view->was_focused_before_grab_)
        OnFocusOut(widget, NULL, host_view);
    }
  }

  static gboolean ButtonPressReleaseEvent(
      GtkWidget* widget, GdkEventButton* event,
      RenderWidgetHostViewGtk* host_view) {
    if (!(event->button == 1 || event->button == 2 || event->button == 3))
      return FALSE;  // We do not forward any other buttons to the renderer.
    if (event->type == GDK_2BUTTON_PRESS || event->type == GDK_3BUTTON_PRESS)
      return FALSE;

    // We want to translate the coordinates of events that do not originate
    // from this widget to be relative to the top left of the widget.
    GtkWidget* event_widget = gtk_get_event_widget(
        reinterpret_cast<GdkEvent*>(event));
    if (event_widget != widget) {
      int x = 0;
      int y = 0;
      gtk_widget_get_pointer(widget, &x, &y);
      // If the mouse event happens outside our popup, force the popup to
      // close.  We do this so a hung renderer doesn't prevent us from
      // releasing the x pointer grab.
      bool click_in_popup = x >= 0 && y >= 0 && x < widget->allocation.width &&
          y < widget->allocation.height;
      // Only Shutdown on mouse downs. Mouse ups can occur outside the render
      // view if the user drags for DnD or while using the scrollbar on a select
      // dropdown. Don't shutdown if we are not a popup.
      if (event->type != GDK_BUTTON_RELEASE && host_view->parent_ &&
          !host_view->is_popup_first_mouse_release_ && !click_in_popup) {
        host_view->host_->Shutdown();
        return FALSE;
      }
      event->x = x;
      event->y = y;
    }

    // TODO(evanm): why is this necessary here but not in test shell?
    // This logic is the same as GtkButton.
    if (event->type == GDK_BUTTON_PRESS && !GTK_WIDGET_HAS_FOCUS(widget))
      gtk_widget_grab_focus(widget);

    host_view->is_popup_first_mouse_release_ = false;
    host_view->GetRenderWidgetHost()->ForwardMouseEvent(
        WebInputEventFactory::mouseEvent(event));

    // Although we did handle the mouse event, we need to let other handlers
    // run (in particular the one installed by TabContentsViewGtk).
    return FALSE;
  }

  static gboolean MouseMoveEvent(GtkWidget* widget, GdkEventMotion* event,
                                 RenderWidgetHostViewGtk* host_view) {
    // We want to translate the coordinates of events that do not originate
    // from this widget to be relative to the top left of the widget.
    GtkWidget* event_widget = gtk_get_event_widget(
        reinterpret_cast<GdkEvent*>(event));
    if (event_widget != widget) {
      int x = 0;
      int y = 0;
      gtk_widget_get_pointer(widget, &x, &y);
      event->x = x;
      event->y = y;
    }
    host_view->GetRenderWidgetHost()->ForwardMouseEvent(
        WebInputEventFactory::mouseEvent(event));
    return FALSE;
  }

  static gboolean CrossingEvent(GtkWidget* widget, GdkEventCrossing* event,
                                RenderWidgetHostViewGtk* host_view) {
    const int any_button_mask =
        GDK_BUTTON1_MASK |
        GDK_BUTTON2_MASK |
        GDK_BUTTON3_MASK |
        GDK_BUTTON4_MASK |
        GDK_BUTTON5_MASK;

    // Only forward crossing events if the mouse button is not down.
    // (When the mouse button is down, the proper events are already being
    // sent by ButtonPressReleaseEvent and MouseMoveEvent, above, and if we
    // additionally send this crossing event with the state indicating the
    // button is down, it causes problems with drag and drop in WebKit.)
    if (!(event->state & any_button_mask)) {
      host_view->GetRenderWidgetHost()->ForwardMouseEvent(
          WebInputEventFactory::mouseEvent(event));
    }

    return FALSE;
  }

  static gboolean MouseScrollEvent(GtkWidget* widget, GdkEventScroll* event,
                                   RenderWidgetHostViewGtk* host_view) {
    // If the user is holding shift, translate it into a horizontal scroll. We
    // don't care what other modifiers the user may be holding (zooming is
    // handled at the TabContentsView level).
    if (event->state & GDK_SHIFT_MASK) {
      if (event->direction == GDK_SCROLL_UP)
        event->direction = GDK_SCROLL_LEFT;
      else if (event->direction == GDK_SCROLL_DOWN)
        event->direction = GDK_SCROLL_RIGHT;
    }

    host_view->GetRenderWidgetHost()->ForwardWheelEvent(
        WebInputEventFactory::mouseWheelEvent(event));
    return FALSE;
  }

  DISALLOW_IMPLICIT_CONSTRUCTORS(RenderWidgetHostViewGtkWidget);
};

// static
RenderWidgetHostView* RenderWidgetHostView::CreateViewForWidget(
    RenderWidgetHost* widget) {
  return new RenderWidgetHostViewGtk(widget);
}

RenderWidgetHostViewGtk::RenderWidgetHostViewGtk(RenderWidgetHost* widget_host)
    : host_(widget_host),
      about_to_validate_and_paint_(false),
      is_hidden_(false),
      is_loading_(false),
      is_showing_context_menu_(false),
      parent_host_view_(NULL),
      parent_(NULL),
      is_popup_first_mouse_release_(true),
      was_focused_before_grab_(false) {
  host_->set_view(this);
}

RenderWidgetHostViewGtk::~RenderWidgetHostViewGtk() {
  view_.Destroy();
}

void RenderWidgetHostViewGtk::InitAsChild() {
  view_.Own(RenderWidgetHostViewGtkWidget::CreateNewWidget(this));
  // |im_context_| must be created after creating |view_| widget.
  im_context_.reset(new GtkIMContextWrapper(this));
  // |key_bindings_handler_| must be created after creating |view_| widget.
  key_bindings_handler_.reset(new GtkKeyBindingsHandler(view_.get()));
  plugin_container_manager_.set_host_widget(view_.get());
  gtk_widget_show(view_.get());

  // Uncomment this line to use out-of-process painting.
  // gpu_view_host_.reset(new GpuViewHost(host_, GetNativeView()));
}

void RenderWidgetHostViewGtk::InitAsPopup(
    RenderWidgetHostView* parent_host_view, const gfx::Rect& pos) {
  parent_host_view_ = parent_host_view;
  parent_ = parent_host_view->GetNativeView();
  GtkWidget* popup = gtk_window_new(GTK_WINDOW_POPUP);
  view_.Own(RenderWidgetHostViewGtkWidget::CreateNewWidget(this));
  // |im_context_| must be created after creating |view_| widget.
  im_context_.reset(new GtkIMContextWrapper(this));
  // |key_bindings_handler_| must be created after creating |view_| widget.
  key_bindings_handler_.reset(new GtkKeyBindingsHandler(view_.get()));
  plugin_container_manager_.set_host_widget(view_.get());
  gtk_container_add(GTK_CONTAINER(popup), view_.get());

  // If we are not activatable, we don't want to grab keyboard input,
  // and webkit will manage our destruction.
  if (activatable()) {
    // Grab all input for the app. If a click lands outside the bounds of the
    // popup, WebKit will notice and destroy us. Before doing this we need
    // to ensure that the the popup is added to the browser's window group,
    // to allow for the grabs to work correctly.
    gtk_window_group_add_window(gtk_window_get_group(
        GTK_WINDOW(gtk_widget_get_toplevel(parent_))), GTK_WINDOW(popup));
    gtk_grab_add(view_.get());

    // Now grab all of X's input.
    gdk_pointer_grab(
        parent_->window,
        TRUE,  // Only events outside of the window are reported with respect
               // to |parent_->window|.
        static_cast<GdkEventMask>(GDK_BUTTON_PRESS_MASK |
            GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK),
        NULL,
        NULL,
        GDK_CURRENT_TIME);
    // We grab keyboard events too so things like alt+tab are eaten.
    gdk_keyboard_grab(parent_->window, TRUE, GDK_CURRENT_TIME);

    // Our parent widget actually keeps GTK focus within its window, but we have
    // to make the webkit selection box disappear to maintain appearances.
    parent_host_view->Blur();
  }

  requested_size_ = gfx::Size(std::min(pos.width(), kMaxWindowWidth),
                              std::min(pos.height(), kMaxWindowHeight));
  host_->WasResized();
  gtk_widget_set_size_request(view_.get(), requested_size_.width(),
                              requested_size_.height());

  gtk_window_set_default_size(GTK_WINDOW(popup), -1, -1);
  // Don't allow the window to be resized. This also forces the window to
  // shrink down to the size of its child contents.
  gtk_window_set_resizable(GTK_WINDOW(popup), FALSE);
  gtk_window_move(GTK_WINDOW(popup), pos.x(), pos.y());
  gtk_widget_show_all(popup);

  // TODO(brettw) possibly enable out-of-process painting here as well
  // (see InitAsChild).
}

void RenderWidgetHostViewGtk::DidBecomeSelected() {
  if (!is_hidden_)
    return;

  if (tab_switch_paint_time_.is_null())
    tab_switch_paint_time_ = base::TimeTicks::Now();
  is_hidden_ = false;
  host_->WasRestored();
}

void RenderWidgetHostViewGtk::WasHidden() {
  if (is_hidden_)
    return;

  // If we receive any more paint messages while we are hidden, we want to
  // ignore them so we don't re-allocate the backing store.  We will paint
  // everything again when we become selected again.
  is_hidden_ = true;

  // If we have a renderer, then inform it that we are being hidden so it can
  // reduce its resource utilization.
  GetRenderWidgetHost()->WasHidden();
}

void RenderWidgetHostViewGtk::SetSize(const gfx::Size& size) {
  // This is called when webkit has sent us a Move message.
  int width = std::min(size.width(), kMaxWindowWidth);
  int height = std::min(size.height(), kMaxWindowHeight);
  if (parent_) {
    // We're a popup, honor the size request.
    gtk_widget_set_size_request(view_.get(), width, height);
  } else {
#if defined(TOOLKIT_VIEWS)
    // TOOLKIT_VIEWS' resize logic flow matches windows. so we go ahead and
    // size the widget.  In GTK+, the size of the widget is determined by it's
    // children.
    gtk_widget_set_size_request(view_.get(), width, height);
#endif
  }
  // Update the size of the RWH.
  if (requested_size_.width() != width ||
      requested_size_.height() != height) {
    requested_size_ = gfx::Size(width, height);
    host_->WasResized();
  }
}

gfx::NativeView RenderWidgetHostViewGtk::GetNativeView() {
  return view_.get();
}

void RenderWidgetHostViewGtk::MovePluginWindows(
    const std::vector<webkit_glue::WebPluginGeometry>& moves) {
  for (size_t i = 0; i < moves.size(); ++i) {
    plugin_container_manager_.MovePluginContainer(moves[i]);
  }
}

void RenderWidgetHostViewGtk::Focus() {
  gtk_widget_grab_focus(view_.get());
}

void RenderWidgetHostViewGtk::Blur() {
  // TODO(estade): We should be clearing native focus as well, but I know of no
  // way to do that without focusing another widget.
  // TODO(estade): it doesn't seem like the CanBlur() check should be necessary
  // since we are only called in response to a ViewHost blur message, but this
  // check is made on Windows so I've added it here as well.
  if (host_->CanBlur())
    host_->Blur();
}

bool RenderWidgetHostViewGtk::HasFocus() {
  return gtk_widget_is_focus(view_.get());
}

void RenderWidgetHostViewGtk::Show() {
  gtk_widget_show(view_.get());
}

void RenderWidgetHostViewGtk::Hide() {
  gtk_widget_hide(view_.get());
}

gfx::Rect RenderWidgetHostViewGtk::GetViewBounds() const {
  GtkAllocation* alloc = &view_.get()->allocation;
  return gfx::Rect(alloc->x, alloc->y,
                   requested_size_.width(),
                   requested_size_.height());
}

void RenderWidgetHostViewGtk::UpdateCursor(const WebCursor& cursor) {
  // Optimize the common case, where the cursor hasn't changed.
  // However, we can switch between different pixmaps, so only on the
  // non-pixmap branch.
  if (current_cursor_.GetCursorType() != GDK_CURSOR_IS_PIXMAP &&
      current_cursor_.GetCursorType() == cursor.GetCursorType()) {
    return;
  }

  current_cursor_ = cursor;
  ShowCurrentCursor();
}

void RenderWidgetHostViewGtk::SetIsLoading(bool is_loading) {
  is_loading_ = is_loading;
  // Only call ShowCurrentCursor() when it will actually change the cursor.
  if (current_cursor_.GetCursorType() == GDK_LAST_CURSOR)
    ShowCurrentCursor();
}

void RenderWidgetHostViewGtk::IMEUpdateStatus(int control,
                                              const gfx::Rect& caret_rect) {
  im_context_->UpdateStatus(control, caret_rect);
  key_bindings_handler_->set_enabled(control != IME_DISABLE);
}

void RenderWidgetHostViewGtk::DidPaintBackingStoreRects(
    const std::vector<gfx::Rect>& rects) {
  if (is_hidden_)
    return;

  for (size_t i = 0; i < rects.size(); ++i) {
    if (about_to_validate_and_paint_) {
      invalid_rect_ = invalid_rect_.Union(rects[i]);
    } else {
      Paint(rects[i]);
    }
  }
}

void RenderWidgetHostViewGtk::DidScrollBackingStoreRect(const gfx::Rect& rect,
                                                        int dx, int dy) {
  if (is_hidden_)
    return;

  // TODO(darin): Implement the equivalent of Win32's ScrollWindowEX.  Can that
  // be done using XCopyArea?  Perhaps similar to
  // BackingStore::ScrollBackingStore?
  if (about_to_validate_and_paint_)
    invalid_rect_ = invalid_rect_.Union(rect);
  else
    Paint(rect);
}

void RenderWidgetHostViewGtk::RenderViewGone() {
  Destroy();
  plugin_container_manager_.set_host_widget(NULL);
}

void RenderWidgetHostViewGtk::Destroy() {
  // If |parent_| is non-null, we are a popup and we must disconnect from our
  // parent and destroy the popup window.
  if (parent_) {
    if (activatable()) {
      GdkDisplay *display = gtk_widget_get_display(parent_);
      gdk_display_pointer_ungrab(display, GDK_CURRENT_TIME);
      gdk_display_keyboard_ungrab(display, GDK_CURRENT_TIME);
      parent_host_view_->Focus();
    }
    gtk_widget_destroy(gtk_widget_get_parent(view_.get()));
  }

  // Remove |view_| from all containers now, so nothing else can hold a
  // reference to |view_|'s widget except possibly a gtk signal handler if
  // this code is currently executing within the context of a gtk signal
  // handler.  Note that |view_| is still alive after this call.  It will be
  // deallocated in the destructor.
  // See http://www.crbug.com/11847 for details.
  gtk_widget_destroy(view_.get());

  // The RenderWidgetHost's destruction led here, so don't call it.
  host_ = NULL;

  MessageLoop::current()->DeleteSoon(FROM_HERE, this);
}

void RenderWidgetHostViewGtk::SetTooltipText(const std::wstring& tooltip_text) {
  // Maximum number of characters we allow in a tooltip.
  const int kMaxTooltipLength = 8 << 10;
  // Clamp the tooltip length to kMaxTooltipLength so that we don't
  // accidentally DOS the user with a mega tooltip (since GTK doesn't do
  // this itself).
  // I filed https://bugzilla.gnome.org/show_bug.cgi?id=604641 upstream.
  const std::wstring& clamped_tooltip =
      l10n_util::TruncateString(tooltip_text, kMaxTooltipLength);

  if (clamped_tooltip.empty()) {
    gtk_widget_set_has_tooltip(view_.get(), FALSE);
  } else {
    gtk_widget_set_tooltip_text(view_.get(),
                                WideToUTF8(clamped_tooltip).c_str());
  }
}

void RenderWidgetHostViewGtk::SelectionChanged(const std::string& text) {
  GtkClipboard* x_clipboard = gtk_clipboard_get(GDK_SELECTION_PRIMARY);
  gtk_clipboard_set_text(x_clipboard, text.c_str(), text.length());
}

void RenderWidgetHostViewGtk::ShowingContextMenu(bool showing) {
  is_showing_context_menu_ = showing;
}

BackingStore* RenderWidgetHostViewGtk::AllocBackingStore(
    const gfx::Size& size) {
  if (gpu_view_host_.get())
    return gpu_view_host_->CreateBackingStore(size);
  return new BackingStoreX(host_, size,
                           x11_util::GetVisualFromGtkWidget(view_.get()),
                           gtk_widget_get_visual(view_.get())->depth);
}

void RenderWidgetHostViewGtk::SetBackground(const SkBitmap& background) {
  RenderWidgetHostView::SetBackground(background);
  host_->Send(new ViewMsg_SetBackground(host_->routing_id(), background));
}

void RenderWidgetHostViewGtk::Paint(const gfx::Rect& damage_rect) {
  GdkWindow* window = view_.get()->window;

  if (gpu_view_host_.get()) {
    // When we're proxying painting, we don't actually display the web page
    // ourselves. We clear it white in case the proxy window isn't visible
    // yet we won't show gibberish.
    if (window)
      gdk_window_clear(window);
    return;
  }

  DCHECK(!about_to_validate_and_paint_);

  invalid_rect_ = damage_rect;
  about_to_validate_and_paint_ = true;
  BackingStoreX* backing_store = static_cast<BackingStoreX*>(
      host_->GetBackingStore(true));
  // Calling GetBackingStore maybe have changed |invalid_rect_|...
  about_to_validate_and_paint_ = false;

  gfx::Rect paint_rect = gfx::Rect(0, 0, kMaxWindowWidth, kMaxWindowHeight);
  paint_rect = paint_rect.Intersect(invalid_rect_);

  if (backing_store) {
    // Only render the widget if it is attached to a window; there's a short
    // period where this object isn't attached to a window but hasn't been
    // Destroy()ed yet and it receives paint messages...
    if (window) {
      backing_store->ShowRect(
          paint_rect, x11_util::GetX11WindowFromGtkWidget(view_.get()));
    }
    if (!whiteout_start_time_.is_null()) {
      base::TimeDelta whiteout_duration = base::TimeTicks::Now() -
          whiteout_start_time_;
      UMA_HISTOGRAM_TIMES("MPArch.RWHH_WhiteoutDuration", whiteout_duration);

      // Reset the start time to 0 so that we start recording again the next
      // time the backing store is NULL...
      whiteout_start_time_ = base::TimeTicks();
    }
    if (!tab_switch_paint_time_.is_null()) {
      base::TimeDelta tab_switch_paint_duration = base::TimeTicks::Now() -
          tab_switch_paint_time_;
      UMA_HISTOGRAM_TIMES("MPArch.RWH_TabSwitchPaintDuration",
          tab_switch_paint_duration);
      // Reset tab_switch_paint_time_ to 0 so future tab selections are
      // recorded.
      tab_switch_paint_time_ = base::TimeTicks();
    }
  } else {
    if (window)
      gdk_window_clear(window);
    if (whiteout_start_time_.is_null())
      whiteout_start_time_ = base::TimeTicks::Now();
  }
}

void RenderWidgetHostViewGtk::ShowCurrentCursor() {
  // The widget may not have a window. If that's the case, abort mission. This
  // is the same issue as that explained above in Paint().
  if (!view_.get()->window)
    return;

  GdkCursor* gdk_cursor;
  switch (current_cursor_.GetCursorType()) {
    case GDK_CURSOR_IS_PIXMAP:
      // TODO(port): WebKit bug https://bugs.webkit.org/show_bug.cgi?id=16388 is
      // that calling gdk_window_set_cursor repeatedly is expensive.  We should
      // avoid it here where possible.
      gdk_cursor = current_cursor_.GetCustomCursor();
      break;

    case GDK_LAST_CURSOR:
      if (is_loading_) {
        // Use MOZ_CURSOR_SPINNING if we are showing the default cursor and
        // the page is loading.
        static const GdkColor fg = { 0, 0, 0, 0 };
        static const GdkColor bg = { 65535, 65535, 65535, 65535 };
        GdkPixmap* source =
            gdk_bitmap_create_from_data(NULL, moz_spinning_bits, 32, 32);
        GdkPixmap* mask =
            gdk_bitmap_create_from_data(NULL, moz_spinning_mask_bits, 32, 32);
        gdk_cursor = gdk_cursor_new_from_pixmap(source, mask, &fg, &bg, 2, 2);
        g_object_unref(source);
        g_object_unref(mask);
      } else {
        gdk_cursor = NULL;
      }
      break;

    default:
      gdk_cursor = gtk_util::GetCursor(
          static_cast<GdkCursorType>(current_cursor_.GetCursorType()));
  }
  gdk_window_set_cursor(view_.get()->window, gdk_cursor);
  // The window now owns the cursor.
  if (gdk_cursor)
    gdk_cursor_unref(gdk_cursor);
}

void RenderWidgetHostViewGtk::CreatePluginContainer(
    gfx::PluginWindowHandle id) {
  plugin_container_manager_.CreatePluginContainer(id);
}

void RenderWidgetHostViewGtk::DestroyPluginContainer(
    gfx::PluginWindowHandle id) {
  plugin_container_manager_.DestroyPluginContainer(id);
}

void RenderWidgetHostViewGtk::ForwardKeyboardEvent(
    const NativeWebKeyboardEvent& event) {
  if (!host_)
    return;

  EditCommands edit_commands;
  if (key_bindings_handler_->Match(event, &edit_commands)) {
    host_->ForwardEditCommandsForNextKeyEvent(edit_commands);
  }
  host_->ForwardKeyboardEvent(event);
}
