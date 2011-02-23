// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/render_widget_host_view_gtk.h"

// If this gets included after the gtk headers, then a bunch of compiler
// errors happen because of a "#define Status int" in Xlib.h, which interacts
// badly with net::URLRequestStatus::Status.
#include "chrome/common/render_messages.h"

#include <cairo/cairo.h>
#include <gdk/gdk.h>
#include <gdk/gdkkeysyms.h>
#include <gdk/gdkx.h>
#include <gtk/gtk.h>

#include <algorithm>
#include <string>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/string_number_conversions.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/renderer_host/backing_store_x.h"
#include "chrome/browser/renderer_host/gtk_im_context_wrapper.h"
#include "chrome/browser/renderer_host/gtk_key_bindings_handler.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/renderer_host/render_view_host_delegate.h"
#include "chrome/browser/renderer_host/render_widget_host.h"
#include "chrome/browser/ui/gtk/gtk_util.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/native_web_keyboard_event.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/gtk/WebInputEventFactory.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/x/x11_util.h"
#include "ui/gfx/gtk_preserve_window.h"
#include "ui/gfx/gtk_native_view_id_manager.h"
#include "webkit/glue/webaccessibility.h"
#include "webkit/glue/webcursor_gtk_data.h"
#include "webkit/plugins/npapi/webplugin.h"

#if defined(OS_CHROMEOS)
#include "views/widget/tooltip_window_gtk.h"
#endif  // defined(OS_CHROMEOS)

namespace {

const int kMaxWindowWidth = 4000;
const int kMaxWindowHeight = 4000;
const char* kRenderWidgetHostViewKey = "__RENDER_WIDGET_HOST_VIEW__";

// The duration of the fade-out animation. See |overlay_animation_|.
const int kFadeEffectDuration = 300;

#if defined(OS_CHROMEOS)
// TODO(davemoore) Under Chromeos we are increasing the rate that the trackpad
// generates events to get better precisions. Eventually we will coordinate the
// driver and this setting to ensure they match.
const float kDefaultScrollPixelsPerTick = 20;
#else
// See WebInputEventFactor.cpp for a reason for this being the default
// scroll size for linux.
const float kDefaultScrollPixelsPerTick = 160.0f / 3.0f;
#endif

}  // namespace

using WebKit::WebInputEventFactory;
using WebKit::WebMouseWheelEvent;

// This class is a simple convenience wrapper for Gtk functions. It has only
// static methods.
class RenderWidgetHostViewGtkWidget {
 public:
  static GtkWidget* CreateNewWidget(RenderWidgetHostViewGtk* host_view) {
    GtkWidget* widget = gtk_preserve_window_new();
    gtk_widget_set_name(widget, "chrome-render-widget-host-view");
    // We manually double-buffer in Paint() because Paint() may or may not be
    // called in repsonse to an "expose-event" signal.
    gtk_widget_set_double_buffered(widget, FALSE);
    gtk_widget_set_redraw_on_allocate(widget, FALSE);
#if defined(NDEBUG)
    gtk_widget_modify_bg(widget, GTK_STATE_NORMAL, &gtk_util::kGdkWhite);
#else
    gtk_widget_modify_bg(widget, GTK_STATE_NORMAL, &gtk_util::kGdkGreen);
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
    g_signal_connect(widget, "client-event",
                     G_CALLBACK(ClientEvent), host_view);


    // Connect after so that we are called after the handler installed by the
    // TabContentsView which handles zoom events.
    g_signal_connect_after(widget, "scroll-event",
                           G_CALLBACK(MouseScrollEvent), host_view);

    g_object_set_data(G_OBJECT(widget), kRenderWidgetHostViewKey,
                      static_cast<RenderWidgetHostView*>(host_view));

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
    if (host_view->IsPopup() && host_view->NeedsInputGrab() &&
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

    return TRUE;
  }

  static gboolean OnFocusOut(GtkWidget* widget, GdkEventFocus* focus,
                             RenderWidgetHostViewGtk* host_view) {
    // Whenever we lose focus, set the cursor back to that of our parent window,
    // which should be the default arrow.
    gdk_window_set_cursor(widget->window, NULL);
    // If we are showing a context menu, maintain the illusion that webkit has
    // focus.
    if (!host_view->is_showing_context_menu_)
      host_view->GetRenderWidgetHost()->Blur();

    // Prevents us from stealing input context focus in OnGrabNotify() handler.
    host_view->was_focused_before_grab_ = false;

    // Disable the GtkIMContext object.
    host_view->im_context_->OnFocusOut();

    return TRUE;
  }

  // Called when we are shadowed or unshadowed by a keyboard grab (which will
  // occur for activatable popups, such as dropdown menus). Popup windows do not
  // take focus, so we never get a focus out or focus in event when they are
  // shown, and must rely on this signal instead.
  static void OnGrabNotify(GtkWidget* widget, gboolean was_grabbed,
                           RenderWidgetHostViewGtk* host_view) {
    if (was_grabbed) {
      if (host_view->was_focused_before_grab_)
        host_view->im_context_->OnFocusIn();
    } else {
      host_view->was_focused_before_grab_ = host_view->HasFocus();
      if (host_view->was_focused_before_grab_) {
        gdk_window_set_cursor(widget->window, NULL);
        host_view->im_context_->OnFocusOut();
      }
    }
  }

  static gboolean ButtonPressReleaseEvent(
      GtkWidget* widget, GdkEventButton* event,
      RenderWidgetHostViewGtk* host_view) {
#if defined (OS_CHROMEOS)
    // We support buttons 8 & 9 for scrolling with an attached USB mouse
    // in ChromeOS. We do this separately from the builtin scrolling support
    // because we want to support the user's expectations about the amount
    // scrolled on each event. xorg.conf on chromeos specifies buttons
    // 8 & 9 for the scroll wheel for the attached USB mouse.
    if (event->type == GDK_BUTTON_RELEASE &&
        (event->button == 8 || event->button == 9)) {
      GdkEventScroll scroll_event;
      scroll_event.type = GDK_SCROLL;
      scroll_event.window = event->window;
      scroll_event.send_event = event->send_event;
      scroll_event.time = event->time;
      scroll_event.x = event->x;
      scroll_event.y = event->y;
      scroll_event.state = event->state;
      if (event->state & GDK_SHIFT_MASK) {
        scroll_event.direction =
            event->button == 8 ? GDK_SCROLL_LEFT : GDK_SCROLL_RIGHT;
      } else {
        scroll_event.direction =
            event->button == 8 ? GDK_SCROLL_UP : GDK_SCROLL_DOWN;
      }
      scroll_event.device = event->device;
      scroll_event.x_root = event->x_root;
      scroll_event.y_root = event->y_root;
      WebMouseWheelEvent web_event =
          WebInputEventFactory::mouseWheelEvent(&scroll_event);
      host_view->GetRenderWidgetHost()->ForwardWheelEvent(web_event);
    }
#endif
    if (!(event->button == 1 || event->button == 2 || event->button == 3))
      return FALSE;  // We do not forward any other buttons to the renderer.
    if (event->type == GDK_2BUTTON_PRESS || event->type == GDK_3BUTTON_PRESS)
      return FALSE;

    // If we don't have focus already, this mouse click will focus us.
    if (!gtk_widget_is_focus(widget))
      host_view->host_->OnMouseActivate();

    // Confirm existing composition text on mouse click events, to make sure
    // the input caret won't be moved with an ongoing composition session.
    host_view->im_context_->ConfirmComposition();

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
      if (event->type != GDK_BUTTON_RELEASE && host_view->IsPopup() &&
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

    host_view->ModifyEventForEdgeDragging(widget, event);
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

  static gboolean ClientEvent(GtkWidget* widget, GdkEventClient* event,
                              RenderWidgetHostViewGtk* host_view) {
    VLOG(1) << "client event type: " << event->message_type
            << " data_format: " << event->data_format
            << " data: " << event->data.l;
    return true;
  }

  // Allow the vertical scroll delta to be overridden from the command line.
  // This will allow us to test more easily to discover the amount
  // (either hard coded or computed) that's best.
  static float GetScrollPixelsPerTick() {
    static float scroll_pixels = -1;
    if (scroll_pixels < 0) {
      // TODO(brettw): Remove the command line switch (crbug.com/63525)
      scroll_pixels = kDefaultScrollPixelsPerTick;
      CommandLine* command_line = CommandLine::ForCurrentProcess();
      std::string scroll_pixels_option =
          command_line->GetSwitchValueASCII(switches::kScrollPixels);
      if (!scroll_pixels_option.empty()) {
        double v;
        if (base::StringToDouble(scroll_pixels_option, &v))
          scroll_pixels = static_cast<float>(v);
      }
      DCHECK_GT(scroll_pixels, 0);
    }
    return scroll_pixels;
  }

  // Return the net up / down (or left / right) distance represented by events
  // in the  events will be removed from the queue. We only look at the top of
  // queue...any other type of event will cause us not to look farther.
  // If there is a change to the set of modifier keys or scroll axis
  // in the events we will stop looking as well.
  static int GetPendingScrollDelta(bool vert, guint current_event_state) {
    int num_clicks = 0;
    GdkEvent* event;
    bool event_coalesced = true;
    while ((event = gdk_event_get()) && event_coalesced) {
      event_coalesced = false;
      if (event->type == GDK_SCROLL) {
        GdkEventScroll scroll = event->scroll;
        if (scroll.state & GDK_SHIFT_MASK) {
          if (scroll.direction == GDK_SCROLL_UP)
            scroll.direction = GDK_SCROLL_LEFT;
          else if (scroll.direction == GDK_SCROLL_DOWN)
            scroll.direction = GDK_SCROLL_RIGHT;
        }
        if (vert) {
          if (scroll.direction == GDK_SCROLL_UP ||
              scroll.direction == GDK_SCROLL_DOWN) {
            if (scroll.state == current_event_state) {
              num_clicks += (scroll.direction == GDK_SCROLL_UP ? 1 : -1);
              gdk_event_free(event);
              event_coalesced = true;
            }
          }
        } else {
          if (scroll.direction == GDK_SCROLL_LEFT ||
              scroll.direction == GDK_SCROLL_RIGHT) {
            if (scroll.state == current_event_state) {
              num_clicks += (scroll.direction == GDK_SCROLL_LEFT ? 1 : -1);
              gdk_event_free(event);
              event_coalesced = true;
            }
          }
        }
      }
    }
    // If we have an event left we put it back on the queue.
    if (event) {
      gdk_event_put(event);
      gdk_event_free(event);
    }
    return num_clicks * GetScrollPixelsPerTick();
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

    WebMouseWheelEvent web_event = WebInputEventFactory::mouseWheelEvent(event);
    // We  peek ahead at the top of the queue to look for additional pending
    // scroll events.
    if (event->direction == GDK_SCROLL_UP ||
        event->direction == GDK_SCROLL_DOWN) {
      if (event->direction == GDK_SCROLL_UP)
        web_event.deltaY = GetScrollPixelsPerTick();
      else
        web_event.deltaY = -GetScrollPixelsPerTick();
      web_event.deltaY += GetPendingScrollDelta(true, event->state);
    } else {
      if (event->direction == GDK_SCROLL_LEFT)
        web_event.deltaX = GetScrollPixelsPerTick();
      else
        web_event.deltaX = -GetScrollPixelsPerTick();
      web_event.deltaX += GetPendingScrollDelta(false, event->state);
    }
    host_view->GetRenderWidgetHost()->ForwardWheelEvent(web_event);
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
      overlay_color_(0),
      overlay_animation_(this),
      parent_(NULL),
      is_popup_first_mouse_release_(true),
      was_focused_before_grab_(false),
      do_x_grab_(false),
      is_fullscreen_(false),
      dragged_at_horizontal_edge_(0),
      dragged_at_vertical_edge_(0),
      accelerated_surface_acquired_(false) {
  host_->set_view(this);
}

RenderWidgetHostViewGtk::~RenderWidgetHostViewGtk() {
  view_.Destroy();
}

void RenderWidgetHostViewGtk::InitAsChild() {
  DoSharedInit();
  overlay_animation_.SetDuration(kFadeEffectDuration);
  overlay_animation_.SetSlideDuration(kFadeEffectDuration);
  gtk_widget_show(view_.get());
}

void RenderWidgetHostViewGtk::InitAsPopup(
    RenderWidgetHostView* parent_host_view, const gfx::Rect& pos) {
  // If we aren't a popup, then |window| will be leaked.
  DCHECK(IsPopup());

  DoSharedInit();
  parent_ = parent_host_view->GetNativeView();
  GtkWindow* window = GTK_WINDOW(gtk_window_new(GTK_WINDOW_POPUP));
  gtk_container_add(GTK_CONTAINER(window), view_.get());
  DoPopupOrFullscreenInit(window, pos);

  // The underlying X window needs to be created and mapped by the above code
  // before we can grab the input devices.
  if (NeedsInputGrab()) {
    // Grab all input for the app. If a click lands outside the bounds of the
    // popup, WebKit will notice and destroy us. Before doing this we need
    // to ensure that the the popup is added to the browser's window group,
    // to allow for the grabs to work correctly.
    gtk_window_group_add_window(gtk_window_get_group(
        GTK_WINDOW(gtk_widget_get_toplevel(parent_))), window);
    gtk_grab_add(view_.get());

    // We need for the application to do an X grab as well. However if the app
    // already has an X grab (as in the case of extension popup), an app grab
    // will suffice.
    do_x_grab_ = !gdk_pointer_is_grabbed();

    // Now grab all of X's input.
    if (do_x_grab_) {
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
    }
  }
}

void RenderWidgetHostViewGtk::InitAsFullscreen() {
  DoSharedInit();

  is_fullscreen_ = true;
  GtkWindow* window = GTK_WINDOW(gtk_window_new(GTK_WINDOW_TOPLEVEL));
  gtk_window_set_decorated(window, FALSE);
  gtk_window_fullscreen(window);
  gtk_container_add(GTK_CONTAINER(window), view_.get());

  // Try to move and resize the window to cover the screen in case the window
  // manager doesn't support _NET_WM_STATE_FULLSCREEN.
  GdkScreen* screen = gtk_window_get_screen(window);
  gfx::Rect bounds(
      0, 0, gdk_screen_get_width(screen), gdk_screen_get_height(screen));
  DoPopupOrFullscreenInit(window, bounds);
}

RenderWidgetHost* RenderWidgetHostViewGtk::GetRenderWidgetHost() const {
  return host_;
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
  if (IsPopup()) {
    // We're a popup, honor the size request.
    gtk_widget_set_size_request(view_.get(), width, height);
  } else {
#if defined(TOOLKIT_VIEWS)
    // TOOLKIT_VIEWS' resize logic flow matches windows. so we go ahead and
    // size the widget.  In GTK+, the size of the widget is determined by its
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
    const std::vector<webkit::npapi::WebPluginGeometry>& moves) {
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

bool RenderWidgetHostViewGtk::IsShowing() {
  // TODO(jcivelli): use gtk_widget_get_visible once we build with GTK 2.18.
  return (GTK_WIDGET_FLAGS(view_.get()) & GTK_VISIBLE) != 0;
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

void RenderWidgetHostViewGtk::ImeUpdateTextInputState(
    WebKit::WebTextInputType type,
    const gfx::Rect& caret_rect) {
  im_context_->UpdateInputMethodState(type, caret_rect);
}

void RenderWidgetHostViewGtk::ImeCancelComposition() {
  im_context_->CancelComposition();
}

void RenderWidgetHostViewGtk::DidUpdateBackingStore(
    const gfx::Rect& scroll_rect, int scroll_dx, int scroll_dy,
    const std::vector<gfx::Rect>& copy_rects) {
  if (is_hidden_)
    return;

  // TODO(darin): Implement the equivalent of Win32's ScrollWindowEX.  Can that
  // be done using XCopyArea?  Perhaps similar to
  // BackingStore::ScrollBackingStore?
  if (about_to_validate_and_paint_)
    invalid_rect_ = invalid_rect_.Union(scroll_rect);
  else
    Paint(scroll_rect);

  for (size_t i = 0; i < copy_rects.size(); ++i) {
    // Avoid double painting.  NOTE: This is only relevant given the call to
    // Paint(scroll_rect) above.
    gfx::Rect rect = copy_rects[i].Subtract(scroll_rect);
    if (rect.IsEmpty())
      continue;

    if (about_to_validate_and_paint_)
      invalid_rect_ = invalid_rect_.Union(rect);
    else
      Paint(rect);
  }
}

void RenderWidgetHostViewGtk::RenderViewGone(base::TerminationStatus status,
                                             int error_code) {
  Destroy();
  plugin_container_manager_.set_host_widget(NULL);
}

void RenderWidgetHostViewGtk::Destroy() {
  if (accelerated_surface_acquired_) {
    GtkNativeViewManager* manager = GtkNativeViewManager::GetInstance();
    gfx::NativeViewId view_id = gfx::IdFromNativeView(GetNativeView());
    gfx::PluginWindowHandle surface = manager->GetXIDForId(&surface, view_id);
    manager->ReleasePermanentXID(surface);
  }

  if (do_x_grab_) {
    // Undo the X grab.
    GdkDisplay* display = gtk_widget_get_display(parent_);
    gdk_display_pointer_ungrab(display, GDK_CURRENT_TIME);
    gdk_display_keyboard_ungrab(display, GDK_CURRENT_TIME);
  }

  // If this is a popup or fullscreen widget, then we need to destroy the window
  // that we created to hold it.
  if (IsPopup() || is_fullscreen_)
    gtk_widget_destroy(gtk_widget_get_parent(view_.get()));

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
  const string16 clamped_tooltip =
      l10n_util::TruncateString(WideToUTF16Hack(tooltip_text),
                                kMaxTooltipLength);

  if (clamped_tooltip.empty()) {
    gtk_widget_set_has_tooltip(view_.get(), FALSE);
  } else {
    gtk_widget_set_tooltip_text(view_.get(),
                                UTF16ToUTF8(clamped_tooltip).c_str());
#if defined(OS_CHROMEOS)
    tooltip_window_->SetTooltipText(UTF16ToWideHack(clamped_tooltip));
#endif  // defined(OS_CHROMEOS)
  }
}

void RenderWidgetHostViewGtk::SelectionChanged(const std::string& text) {
  if (!text.empty()) {
    GtkClipboard* x_clipboard = gtk_clipboard_get(GDK_SELECTION_PRIMARY);
    gtk_clipboard_set_text(x_clipboard, text.c_str(), text.length());
  }
}

void RenderWidgetHostViewGtk::ShowingContextMenu(bool showing) {
  is_showing_context_menu_ = showing;
}

#if !defined(TOOLKIT_VIEWS)
void RenderWidgetHostViewGtk::AppendInputMethodsContextMenu(MenuGtk* menu) {
  im_context_->AppendInputMethodsContextMenu(menu);
}
#endif

bool RenderWidgetHostViewGtk::NeedsInputGrab() {
  return popup_type_ == WebKit::WebPopupTypeSelect;
}

bool RenderWidgetHostViewGtk::IsPopup() const {
  return popup_type_ != WebKit::WebPopupTypeNone;
}

void RenderWidgetHostViewGtk::DoSharedInit() {
  view_.Own(RenderWidgetHostViewGtkWidget::CreateNewWidget(this));
  im_context_.reset(new GtkIMContextWrapper(this));
  key_bindings_handler_.reset(new GtkKeyBindingsHandler(view_.get()));
  plugin_container_manager_.set_host_widget(view_.get());
#if defined(OS_CHROMEOS)
  tooltip_window_.reset(new views::TooltipWindowGtk(view_.get()));
#endif
}

void RenderWidgetHostViewGtk::DoPopupOrFullscreenInit(GtkWindow* window,
                                                      const gfx::Rect& bounds) {
  requested_size_.SetSize(std::min(bounds.width(), kMaxWindowWidth),
                          std::min(bounds.height(), kMaxWindowHeight));
  host_->WasResized();

  gtk_widget_set_size_request(
      view_.get(), requested_size_.width(), requested_size_.height());

  // Don't allow the window to be resized. This also forces the window to
  // shrink down to the size of its child contents.
  gtk_window_set_resizable(window, FALSE);
  gtk_window_set_default_size(window, -1, -1);
  gtk_window_move(window, bounds.x(), bounds.y());

  gtk_widget_show_all(GTK_WIDGET(window));
}

BackingStore* RenderWidgetHostViewGtk::AllocBackingStore(
    const gfx::Size& size) {
  return new BackingStoreX(host_, size,
                           ui::GetVisualFromGtkWidget(view_.get()),
                           gtk_widget_get_visual(view_.get())->depth);
}

void RenderWidgetHostViewGtk::SetBackground(const SkBitmap& background) {
  RenderWidgetHostView::SetBackground(background);
  host_->Send(new ViewMsg_SetBackground(host_->routing_id(), background));
}

void RenderWidgetHostViewGtk::ModifyEventForEdgeDragging(
    GtkWidget* widget, GdkEventMotion* event) {
  // If the widget is aligned with an edge of the monitor its on and the user
  // attempts to drag past that edge we track the number of times it has
  // occurred, so that we can force the widget to scroll when it otherwise
  // would be unable to, by modifying the (x,y) position in the drag
  // event that we forward on to webkit. If we get a move that's no longer a
  // drag or a drag indicating the user is no longer at that edge we stop
  // altering the drag events.
  int new_dragged_at_horizontal_edge = 0;
  int new_dragged_at_vertical_edge = 0;
  // Used for checking the edges of the monitor. We cache the values to save
  // roundtrips to the X server.
  static gfx::Size drag_monitor_size;
  if (event->state & GDK_BUTTON1_MASK) {
    if (drag_monitor_size.IsEmpty()) {
      // We can safely cache the monitor size for the duration of a drag.
      GdkScreen* screen = gtk_widget_get_screen(widget);
      int monitor =
          gdk_screen_get_monitor_at_point(screen, event->x_root, event->y_root);
      GdkRectangle geometry;
      gdk_screen_get_monitor_geometry(screen, monitor, &geometry);
      drag_monitor_size.SetSize(geometry.width, geometry.height);
    }

    // Check X and Y independently, as the user could be dragging into a corner.
    if (event->x == 0 && event->x_root == 0) {
      new_dragged_at_horizontal_edge = dragged_at_horizontal_edge_ - 1;
    } else if (widget->allocation.width - 1 == static_cast<gint>(event->x) &&
        drag_monitor_size.width() - 1 == static_cast<gint>(event->x_root)) {
      new_dragged_at_horizontal_edge = dragged_at_horizontal_edge_ + 1;
    }

    if (event->y == 0 && event->y_root == 0) {
      new_dragged_at_vertical_edge = dragged_at_vertical_edge_ - 1;
    } else if (widget->allocation.height - 1 == static_cast<gint>(event->y) &&
        drag_monitor_size.height() - 1 == static_cast<gint>(event->y_root)) {
      new_dragged_at_vertical_edge = dragged_at_vertical_edge_ + 1;
    }

    event->x_root += new_dragged_at_horizontal_edge;
    event->x += new_dragged_at_horizontal_edge;
    event->y_root += new_dragged_at_vertical_edge;
    event->y += new_dragged_at_vertical_edge;
  } else {
    // Clear whenever we get a non-drag mouse move.
    drag_monitor_size.SetSize(0, 0);
  }
  dragged_at_horizontal_edge_ = new_dragged_at_horizontal_edge;
  dragged_at_vertical_edge_ = new_dragged_at_vertical_edge;
}

void RenderWidgetHostViewGtk::Paint(const gfx::Rect& damage_rect) {
  // If the GPU process is rendering directly into the View,
  // call the compositor directly.
  RenderWidgetHost* render_widget_host = GetRenderWidgetHost();
  if (render_widget_host->is_accelerated_compositing_active()) {
    host_->ScheduleComposite();
    return;
  }

  GdkWindow* window = view_.get()->window;
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
      if (SkColorGetA(overlay_color_) == 0) {
        // In the common case, use XCopyArea. We don't draw more than once, so
        // we don't need to double buffer.
        backing_store->XShowRect(gfx::Point(0, 0),
            paint_rect, ui::GetX11WindowFromGtkWidget(view_.get()));
      } else {
        // If the grey blend is showing, we make two drawing calls. Use double
        // buffering to prevent flicker. Use CairoShowRect because XShowRect
        // shortcuts GDK's double buffering. We won't be able to draw outside
        // of |damage_rect|, so invalidate the difference between |paint_rect|
        // and |damage_rect|.
        if (paint_rect != damage_rect) {
          GdkRectangle extra_gdk_rect =
              paint_rect.Subtract(damage_rect).ToGdkRectangle();
          gdk_window_invalidate_rect(window, &extra_gdk_rect, false);
        }

        GdkRectangle rect = { damage_rect.x(), damage_rect.y(),
                              damage_rect.width(), damage_rect.height() };
        gdk_window_begin_paint_rect(window, &rect);

        backing_store->CairoShowRect(damage_rect, GDK_DRAWABLE(window));

        cairo_t* cr = gdk_cairo_create(window);
        gdk_cairo_rectangle(cr, &rect);
        SkColor overlay = SkColorSetA(
            overlay_color_,
            SkColorGetA(overlay_color_) *
                overlay_animation_.GetCurrentValue());
        float r = SkColorGetR(overlay) / 255.;
        float g = SkColorGetG(overlay) / 255.;
        float b = SkColorGetB(overlay) / 255.;
        float a = SkColorGetA(overlay) / 255.;
        cairo_set_source_rgba(cr, r, g, b, a);
        cairo_fill(cr);
        cairo_destroy(cr);

        gdk_window_end_paint(window);
      }
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

  // TODO(port): WebKit bug https://bugs.webkit.org/show_bug.cgi?id=16388 is
  // that calling gdk_window_set_cursor repeatedly is expensive.  We should
  // avoid it here where possible.
  GdkCursor* gdk_cursor;
  bool should_unref = false;
  if (current_cursor_.GetCursorType() == GDK_LAST_CURSOR) {
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
      should_unref = true;
      g_object_unref(source);
      g_object_unref(mask);
    } else {
      gdk_cursor = NULL;
    }
  } else {
    gdk_cursor = current_cursor_.GetNativeCursor();
  }
  gdk_window_set_cursor(view_.get()->window, gdk_cursor);
  // The window now owns the cursor.
  if (should_unref && gdk_cursor)
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

void RenderWidgetHostViewGtk::SetVisuallyDeemphasized(
    const SkColor* color, bool animate) {
  // Do nothing unless |color| has changed, meaning |animate| is only
  // respected for the first call.
  if (color && (*color == overlay_color_))
    return;

  overlay_color_ = color ? *color : 0;

  if (animate) {
    overlay_animation_.Reset();
    overlay_animation_.Show();
  } else {
    overlay_animation_.Reset(1.0);
    gtk_widget_queue_draw(view_.get());
  }
}

bool RenderWidgetHostViewGtk::ContainsNativeView(
    gfx::NativeView native_view) const {
  // TODO(port)
  NOTREACHED() <<
    "RenderWidgetHostViewGtk::ContainsNativeView not implemented.";
  return false;
}

void RenderWidgetHostViewGtk::AcceleratedCompositingActivated(bool activated) {
  GtkPreserveWindow* widget =
    reinterpret_cast<GtkPreserveWindow*>(view_.get());

  gtk_preserve_window_delegate_resize(widget, activated);
}

gfx::PluginWindowHandle RenderWidgetHostViewGtk::AcquireCompositingSurface() {
  GtkNativeViewManager* manager = GtkNativeViewManager::GetInstance();
  gfx::PluginWindowHandle surface = gfx::kNullPluginWindow;
  gfx::NativeViewId view_id = gfx::IdFromNativeView(GetNativeView());

  if (!manager->GetPermanentXIDForId(&surface, view_id)) {
    DLOG(ERROR) << "Can't find XID for view id " << view_id;
  } else {
    accelerated_surface_acquired_ = true;
  }
  return surface;
}

void RenderWidgetHostViewGtk::ReleaseCompositingSurface(
    gfx::PluginWindowHandle surface) {
  GtkNativeViewManager* manager = GtkNativeViewManager::GetInstance();
  manager->ReleasePermanentXID(surface);
  accelerated_surface_acquired_ = false;
}

void RenderWidgetHostViewGtk::ForwardKeyboardEvent(
    const NativeWebKeyboardEvent& event) {
  if (!host_)
    return;

  EditCommands edit_commands;
  if (!event.skip_in_browser &&
      key_bindings_handler_->Match(event, &edit_commands)) {
    host_->ForwardEditCommandsForNextKeyEvent(edit_commands);
    NativeWebKeyboardEvent copy_event(event);
    copy_event.match_edit_command = true;
    host_->ForwardKeyboardEvent(copy_event);
    return;
  }

  host_->ForwardKeyboardEvent(event);
}

void RenderWidgetHostViewGtk::AnimationEnded(const ui::Animation* animation) {
  gtk_widget_queue_draw(view_.get());
}

void RenderWidgetHostViewGtk::AnimationProgressed(
    const ui::Animation* animation) {
  gtk_widget_queue_draw(view_.get());
}

void RenderWidgetHostViewGtk::AnimationCanceled(
    const ui::Animation* animation) {
  gtk_widget_queue_draw(view_.get());
}

// static
RenderWidgetHostView*
    RenderWidgetHostView::GetRenderWidgetHostViewFromNativeView(
        gfx::NativeView widget) {
  gpointer user_data = g_object_get_data(G_OBJECT(widget),
                                         kRenderWidgetHostViewKey);
  return reinterpret_cast<RenderWidgetHostView*>(user_data);
}
