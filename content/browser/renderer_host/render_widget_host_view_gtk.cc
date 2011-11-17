// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_widget_host_view_gtk.h"

// If this gets included after the gtk headers, then a bunch of compiler
// errors happen because of a "#define Status int" in Xlib.h, which interacts
// badly with net::URLRequestStatus::Status.
#include "content/common/view_messages.h"

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
#include "content/browser/renderer_host/backing_store_gtk.h"
#include "content/browser/renderer_host/gtk_im_context_wrapper.h"
#include "content/browser/renderer_host/gtk_window_utils.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/renderer_host/render_view_host_delegate.h"
#include "content/browser/renderer_host/render_widget_host.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "content/public/common/content_switches.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebInputEvent.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebScreenInfo.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/gtk/WebInputEventFactory.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/x11/WebScreenInfoFactory.h"
#include "ui/base/text/text_elider.h"
#include "ui/base/x/x11_util.h"
#include "ui/gfx/gtk_native_view_id_manager.h"
#include "ui/gfx/gtk_preserve_window.h"
#include "webkit/glue/webaccessibility.h"
#include "webkit/glue/webcursor_gtk_data.h"
#include "webkit/plugins/npapi/webplugin.h"

#if defined(OS_CHROMEOS)
#include "ui/base/gtk/tooltip_window_gtk.h"
#else
#include "content/browser/renderer_host/gtk_key_bindings_handler.h"
#endif  // defined(OS_CHROMEOS)

namespace {

const int kMaxWindowWidth = 4000;
const int kMaxWindowHeight = 4000;

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

const GdkColor kBGColor =
#if defined(NDEBUG)
    { 0, 0xff * 257, 0xff * 257, 0xff * 257 };
#else
    { 0, 0x00 * 257, 0xff * 257, 0x00 * 257 };
#endif

// Returns the spinning cursor used for loading state.
GdkCursor* GetMozSpinningCursor() {
  static GdkCursor* moz_spinning_cursor = NULL;
  if (!moz_spinning_cursor) {
    const GdkColor fg = { 0, 0, 0, 0 };
    const GdkColor bg = { 65535, 65535, 65535, 65535 };
    GdkPixmap* source =
        gdk_bitmap_create_from_data(NULL, moz_spinning_bits, 32, 32);
    GdkPixmap* mask =
        gdk_bitmap_create_from_data(NULL, moz_spinning_mask_bits, 32, 32);
    moz_spinning_cursor =
        gdk_cursor_new_from_pixmap(source, mask, &fg, &bg, 2, 2);
    g_object_unref(source);
    g_object_unref(mask);
  }
  return moz_spinning_cursor;
}

bool MovedToCenter(const WebKit::WebMouseEvent& mouse_event,
                   const gfx::Point& center) {
  return mouse_event.globalX == center.x() &&
         mouse_event.globalY == center.y();
}

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
    gtk_widget_modify_bg(widget, GTK_STATE_NORMAL, &kBGColor);
    // Allow the browser window to be resized freely.
    gtk_widget_set_size_request(widget, 0, 0);

    gtk_widget_add_events(widget, GDK_EXPOSURE_MASK |
                                  GDK_STRUCTURE_MASK |
                                  GDK_POINTER_MOTION_MASK |
                                  GDK_BUTTON_PRESS_MASK |
                                  GDK_BUTTON_RELEASE_MASK |
                                  GDK_KEY_PRESS_MASK |
                                  GDK_KEY_RELEASE_MASK |
                                  GDK_FOCUS_CHANGE_MASK |
                                  GDK_ENTER_NOTIFY_MASK |
                                  GDK_LEAVE_NOTIFY_MASK);
    gtk_widget_set_can_focus(widget, TRUE);

    g_signal_connect(widget, "expose-event",
                     G_CALLBACK(OnExposeEvent), host_view);
    g_signal_connect(widget, "realize",
                     G_CALLBACK(OnRealize), host_view);
    g_signal_connect(widget, "configure-event",
                     G_CALLBACK(OnConfigureEvent), host_view);
    g_signal_connect(widget, "key-press-event",
                     G_CALLBACK(OnKeyPressReleaseEvent), host_view);
    g_signal_connect(widget, "key-release-event",
                     G_CALLBACK(OnKeyPressReleaseEvent), host_view);
    g_signal_connect(widget, "focus-in-event",
                     G_CALLBACK(OnFocusIn), host_view);
    g_signal_connect(widget, "focus-out-event",
                     G_CALLBACK(OnFocusOut), host_view);
    g_signal_connect(widget, "grab-notify",
                     G_CALLBACK(OnGrabNotify), host_view);
    g_signal_connect(widget, "button-press-event",
                     G_CALLBACK(OnButtonPressReleaseEvent), host_view);
    g_signal_connect(widget, "button-release-event",
                     G_CALLBACK(OnButtonPressReleaseEvent), host_view);
    g_signal_connect(widget, "motion-notify-event",
                     G_CALLBACK(OnMouseMoveEvent), host_view);
    g_signal_connect(widget, "enter-notify-event",
                     G_CALLBACK(OnCrossingEvent), host_view);
    g_signal_connect(widget, "leave-notify-event",
                     G_CALLBACK(OnCrossingEvent), host_view);
    g_signal_connect(widget, "client-event",
                     G_CALLBACK(OnClientEvent), host_view);


    // Connect after so that we are called after the handler installed by the
    // TabContentsView which handles zoom events.
    g_signal_connect_after(widget, "scroll-event",
                           G_CALLBACK(OnMouseScrollEvent), host_view);

    return widget;
  }

 private:
  static gboolean OnExposeEvent(GtkWidget* widget,
                                GdkEventExpose* expose,
                                RenderWidgetHostViewGtk* host_view) {
    if (host_view->is_hidden_)
      return FALSE;
    const gfx::Rect damage_rect(expose->area);
    host_view->Paint(damage_rect);
    return FALSE;
  }

  static gboolean OnRealize(GtkWidget* widget,
                            RenderWidgetHostViewGtk* host_view) {
    // Use GtkSignalRegistrar to register events on a widget we don't
    // control the lifetime of, auto disconnecting at our end of our life.
    host_view->signals_.Connect(gtk_widget_get_toplevel(widget),
                                "configure-event",
                                G_CALLBACK(OnConfigureEvent), host_view);
    return FALSE;
  }

  static gboolean OnConfigureEvent(GtkWidget* widget,
                                   GdkEventConfigure* event,
                                   RenderWidgetHostViewGtk* host_view) {
    host_view->widget_center_valid_ = false;
    host_view->mouse_has_been_warped_to_new_center_ = false;
    return FALSE;
  }

  static gboolean OnKeyPressReleaseEvent(GtkWidget* widget,
                                         GdkEventKey* event,
                                         RenderWidgetHostViewGtk* host_view) {
    // Force popups or fullscreen windows to close on Escape so they won't keep
    // the keyboard grabbed or be stuck onscreen if the renderer is hanging.
    bool should_close_on_escape =
        (host_view->IsPopup() && host_view->NeedsInputGrab()) ||
        host_view->is_fullscreen_;
    if (should_close_on_escape && GDK_Escape == event->keyval) {
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

  static gboolean OnFocusIn(GtkWidget* widget,
                            GdkEventFocus* focus,
                            RenderWidgetHostViewGtk* host_view) {
    host_view->ShowCurrentCursor();
    host_view->GetRenderWidgetHost()->GotFocus();
    host_view->GetRenderWidgetHost()->SetActive(true);

    // The only way to enable a GtkIMContext object is to call its focus in
    // handler.
    host_view->im_context_->OnFocusIn();

    return TRUE;
  }

  static gboolean OnFocusOut(GtkWidget* widget,
                             GdkEventFocus* focus,
                             RenderWidgetHostViewGtk* host_view) {
    // Whenever we lose focus, set the cursor back to that of our parent window,
    // which should be the default arrow.
    gdk_window_set_cursor(widget->window, NULL);
    // If we are showing a context menu, maintain the illusion that webkit has
    // focus.
    if (!host_view->is_showing_context_menu_) {
      host_view->GetRenderWidgetHost()->SetActive(false);
      host_view->GetRenderWidgetHost()->Blur();
    }

    // Prevents us from stealing input context focus in OnGrabNotify() handler.
    host_view->was_imcontext_focused_before_grab_ = false;

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
      if (host_view->was_imcontext_focused_before_grab_)
        host_view->im_context_->OnFocusIn();
    } else {
      host_view->was_imcontext_focused_before_grab_ =
          host_view->im_context_->is_focused();
      if (host_view->was_imcontext_focused_before_grab_) {
        gdk_window_set_cursor(widget->window, NULL);
        host_view->im_context_->OnFocusOut();
      }
    }
  }

  static gboolean OnButtonPressReleaseEvent(
      GtkWidget* widget,
      GdkEventButton* event,
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

    if (event->type != GDK_BUTTON_RELEASE)
      host_view->set_last_mouse_down(event);

    if (!(event->button == 1 || event->button == 2 || event->button == 3))
      return FALSE;  // We do not forward any other buttons to the renderer.
    if (event->type == GDK_2BUTTON_PRESS || event->type == GDK_3BUTTON_PRESS)
      return FALSE;

    // If we don't have focus already, this mouse click will focus us.
    if (!gtk_widget_is_focus(widget))
      host_view->host_->OnMouseActivate();

    // Confirm existing composition text on mouse click events, to make sure
    // the input caret won't be moved with an ongoing composition session.
    if (event->type != GDK_BUTTON_RELEASE)
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
    if (event->type == GDK_BUTTON_PRESS && !gtk_widget_has_focus(widget))
      gtk_widget_grab_focus(widget);

    host_view->is_popup_first_mouse_release_ = false;
    RenderWidgetHost* widget_host = host_view->GetRenderWidgetHost();
    if (widget_host)
      widget_host->ForwardMouseEvent(WebInputEventFactory::mouseEvent(event));

    // Although we did handle the mouse event, we need to let other handlers
    // run (in particular the one installed by TabContentsViewGtk).
    return FALSE;
  }

  static gboolean OnMouseMoveEvent(GtkWidget* widget,
                                   GdkEventMotion* event,
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

    WebKit::WebMouseEvent mouse_event =
        WebInputEventFactory::mouseEvent(event);

    if (host_view->mouse_locked_) {
      gfx::Point center = host_view->GetWidgetCenter();

      bool moved_to_center = MovedToCenter(mouse_event, center);
      if (moved_to_center)
        host_view->mouse_has_been_warped_to_new_center_ = true;

      host_view->ModifyEventMovementAndCoords(&mouse_event);

      if (!moved_to_center &&
          (mouse_event.movementX || mouse_event.movementY)) {
        GdkDisplay* display = gtk_widget_get_display(widget);
        GdkScreen* screen = gtk_widget_get_screen(widget);
        gdk_display_warp_pointer(display, screen, center.x(), center.y());
        if (host_view->mouse_has_been_warped_to_new_center_)
          host_view->GetRenderWidgetHost()->ForwardMouseEvent(mouse_event);
      }
    } else {  // Mouse is not locked.
      host_view->ModifyEventMovementAndCoords(&mouse_event);
      host_view->GetRenderWidgetHost()->ForwardMouseEvent(mouse_event);
    }
    return FALSE;
  }

  static gboolean OnCrossingEvent(GtkWidget* widget,
                                  GdkEventCrossing* event,
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
      WebKit::WebMouseEvent mouse_event =
          WebInputEventFactory::mouseEvent(event);
      host_view->ModifyEventMovementAndCoords(&mouse_event);
      // When crossing out and back into a render view the movement values
      // must represent the instantaneous movement of the mouse, not the jump
      // from the exit to re-entry point.
      mouse_event.movementX = 0;
      mouse_event.movementY = 0;
      host_view->GetRenderWidgetHost()->ForwardMouseEvent(mouse_event);
    }

    return FALSE;
  }

  static gboolean OnClientEvent(GtkWidget* widget,
                                GdkEventClient* event,
                                RenderWidgetHostViewGtk* host_view) {
    VLOG(1) << "client event type: " << event->message_type
            << " data_format: " << event->data_format
            << " data: " << event->data.l;
    return TRUE;
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

  static gboolean OnMouseScrollEvent(GtkWidget* widget,
                                     GdkEventScroll* event,
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
      was_imcontext_focused_before_grab_(false),
      do_x_grab_(false),
      is_fullscreen_(false),
      destroy_handler_id_(0),
      dragged_at_horizontal_edge_(0),
      dragged_at_vertical_edge_(0),
      compositing_surface_(gfx::kNullPluginWindow),
      last_mouse_down_(NULL) {
  host_->SetView(this);
}

RenderWidgetHostViewGtk::~RenderWidgetHostViewGtk() {
  UnlockMouse();
  set_last_mouse_down(NULL);
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

void RenderWidgetHostViewGtk::InitAsFullscreen(
    RenderWidgetHostView* /*reference_host_view*/) {
  DoSharedInit();

  is_fullscreen_ = true;
  GtkWindow* window = GTK_WINDOW(gtk_window_new(GTK_WINDOW_TOPLEVEL));
  gtk_window_set_decorated(window, FALSE);
  gtk_window_fullscreen(window);
  g_signal_connect(GTK_WIDGET(window),
                   "window-state-event",
                   G_CALLBACK(&OnWindowStateEventThunk),
                   this);
  destroy_handler_id_ = g_signal_connect(GTK_WIDGET(window),
                                         "destroy",
                                         G_CALLBACK(OnDestroyThunk),
                                         this);
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

void RenderWidgetHostViewGtk::SetBounds(const gfx::Rect& rect) {
  // This is called when webkit has sent us a Move message.
  if (IsPopup()) {
    gtk_window_move(GTK_WINDOW(gtk_widget_get_toplevel(view_.get())),
                    rect.x(), rect.y());
  }

  SetSize(rect.size());
}

gfx::NativeView RenderWidgetHostViewGtk::GetNativeView() const {
  return view_.get();
}

gfx::NativeViewId RenderWidgetHostViewGtk::GetNativeViewId() const {
  return GtkNativeViewManager::GetInstance()->GetIdForWidget(view_.get());
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

bool RenderWidgetHostViewGtk::HasFocus() const {
  return gtk_widget_is_focus(view_.get());
}

void RenderWidgetHostViewGtk::Show() {
  gtk_widget_show(view_.get());
}

void RenderWidgetHostViewGtk::Hide() {
  gtk_widget_hide(view_.get());
}

bool RenderWidgetHostViewGtk::IsShowing() {
  return gtk_widget_get_visible(view_.get());
}

gfx::Rect RenderWidgetHostViewGtk::GetViewBounds() const {
  GdkWindow* gdk_window = view_.get()->window;
  if (!gdk_window)
    return gfx::Rect(requested_size_);
  GdkRectangle window_rect;
  gdk_window_get_origin(gdk_window, &window_rect.x, &window_rect.y);
  return gfx::Rect(window_rect.x, window_rect.y,
                   requested_size_.width(), requested_size_.height());
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

void RenderWidgetHostViewGtk::TextInputStateChanged(
    ui::TextInputType type,
    bool can_compose_inline) {
  im_context_->UpdateInputMethodState(type, can_compose_inline);
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
  if (compositing_surface_ != gfx::kNullPluginWindow) {
    GtkNativeViewManager* manager = GtkNativeViewManager::GetInstance();
    manager->ReleasePermanentXID(compositing_surface_);
  }

  if (do_x_grab_) {
    // Undo the X grab.
    GdkDisplay* display = gtk_widget_get_display(parent_);
    gdk_display_pointer_ungrab(display, GDK_CURRENT_TIME);
    gdk_display_keyboard_ungrab(display, GDK_CURRENT_TIME);
  }

  // If this is a popup or fullscreen widget, then we need to destroy the window
  // that we created to hold it.
  if (IsPopup() || is_fullscreen_) {
    GtkWidget* window = gtk_widget_get_parent(view_.get());

    // Disconnect the destroy handler so that we don't try to shutdown twice.
    if (is_fullscreen_)
      g_signal_handler_disconnect(window, destroy_handler_id_);

    gtk_widget_destroy(window);
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

void RenderWidgetHostViewGtk::SetTooltipText(const string16& tooltip_text) {
  // Maximum number of characters we allow in a tooltip.
  const int kMaxTooltipLength = 8 << 10;
  // Clamp the tooltip length to kMaxTooltipLength so that we don't
  // accidentally DOS the user with a mega tooltip (since GTK doesn't do
  // this itself).
  // I filed https://bugzilla.gnome.org/show_bug.cgi?id=604641 upstream.
  const string16 clamped_tooltip =
      ui::TruncateString(tooltip_text, kMaxTooltipLength);

  if (clamped_tooltip.empty()) {
    gtk_widget_set_has_tooltip(view_.get(), FALSE);
  } else {
    gtk_widget_set_tooltip_text(view_.get(),
                                UTF16ToUTF8(clamped_tooltip).c_str());
#if defined(OS_CHROMEOS)
    tooltip_window_->SetTooltipText(clamped_tooltip);
#endif  // defined(OS_CHROMEOS)
  }
}

void RenderWidgetHostViewGtk::SelectionChanged(const string16& text,
                                               size_t offset,
                                               const ui::Range& range) {
  if (text.empty() || range.is_empty())
    return;
  size_t pos = range.GetMin() - offset;
  size_t n = range.length();

  DCHECK(pos + n <= text.length()) << "The text can not fully cover range.";
  if (pos >= text.length()) {
    NOTREACHED() << "The text can not cover range.";
    return;
  }

  selection_text_ = text;
  selection_text_offset_ = offset;
  selection_range_ = range;

  std::string utf8_selection = UTF16ToUTF8(text.substr(pos, n));
  GtkClipboard* x_clipboard = gtk_clipboard_get(GDK_SELECTION_PRIMARY);
  gtk_clipboard_set_text(
      x_clipboard, utf8_selection.c_str(), utf8_selection.length());
}

void RenderWidgetHostViewGtk::SelectionBoundsChanged(
    const gfx::Rect& start_rect,
    const gfx::Rect& end_rect) {
  im_context_->UpdateCaretBounds(start_rect.Union(end_rect));
}

void RenderWidgetHostViewGtk::ShowingContextMenu(bool showing) {
  is_showing_context_menu_ = showing;
}

#if !defined(TOOLKIT_VIEWS)
GtkWidget* RenderWidgetHostViewGtk::BuildInputMethodsGtkMenu() {
  return im_context_->BuildInputMethodsGtkMenu();
}
#endif

gboolean RenderWidgetHostViewGtk::OnWindowStateEvent(
    GtkWidget* widget,
    GdkEventWindowState* event) {
  if (is_fullscreen_) {
    // If a fullscreen widget got unfullscreened (e.g. by the window manager),
    // close it.
    bool unfullscreened =
        (event->changed_mask & GDK_WINDOW_STATE_FULLSCREEN) &&
        !(event->new_window_state & GDK_WINDOW_STATE_FULLSCREEN);
    if (unfullscreened) {
      host_->Shutdown();
      return TRUE;
    }
  }

  return FALSE;
}

void RenderWidgetHostViewGtk::OnDestroy(GtkWidget* widget) {
  DCHECK(is_fullscreen_);
  host_->Shutdown();
}

bool RenderWidgetHostViewGtk::NeedsInputGrab() {
  return popup_type_ == WebKit::WebPopupTypeSelect;
}

bool RenderWidgetHostViewGtk::IsPopup() const {
  return popup_type_ != WebKit::WebPopupTypeNone;
}

void RenderWidgetHostViewGtk::DoSharedInit() {
  view_.Own(RenderWidgetHostViewGtkWidget::CreateNewWidget(this));
  im_context_.reset(new GtkIMContextWrapper(this));
  plugin_container_manager_.set_host_widget(view_.get());
#if defined(OS_CHROMEOS)
  tooltip_window_.reset(new ui::TooltipWindowGtk(view_.get()));
#else
  key_bindings_handler_.reset(new GtkKeyBindingsHandler(view_.get()));
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
  return new BackingStoreGtk(host_, size,
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
  CR_DEFINE_STATIC_LOCAL(gfx::Size, drag_monitor_size, ());
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

  // If the size of our canvas is (0,0), then we don't want to block here. We
  // are doing one of our first paints and probably have animations going on.
  bool force_create = !host_->empty();
  BackingStoreGtk* backing_store = static_cast<BackingStoreGtk*>(
      host_->GetBackingStore(force_create));
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
  if (current_cursor_.GetCursorType() == GDK_LAST_CURSOR) {
    // Use MOZ_CURSOR_SPINNING if we are showing the default cursor and
    // the page is loading.
    gdk_cursor = is_loading_ ? GetMozSpinningCursor() : NULL;
  } else {
    gdk_cursor = current_cursor_.GetNativeCursor();
  }
  gdk_window_set_cursor(view_.get()->window, gdk_cursor);
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

void RenderWidgetHostViewGtk::UnhandledWheelEvent(
    const WebKit::WebMouseWheelEvent& event) {
}

void RenderWidgetHostViewGtk::SetHasHorizontalScrollbar(
    bool has_horizontal_scrollbar) {
}

void RenderWidgetHostViewGtk::SetScrollOffsetPinning(
    bool is_pinned_to_left, bool is_pinned_to_right) {
}


void RenderWidgetHostViewGtk::OnAcceleratedCompositingStateChange() {
  bool activated = host_->is_accelerated_compositing_active();
  GtkPreserveWindow* widget = reinterpret_cast<GtkPreserveWindow*>(view_.get());

  gtk_preserve_window_delegate_resize(widget, activated);
}

void RenderWidgetHostViewGtk::GetScreenInfo(WebKit::WebScreenInfo* results) {
  GdkWindow* gdk_window = view_.get()->window;
  if (!gdk_window) {
    GdkDisplay* display = gdk_display_get_default();
    gdk_window = gdk_display_get_default_group(display);
  }
  if (!gdk_window)
    return;
  content::GetScreenInfoFromNativeWindow(gdk_window, results);
}

gfx::Rect RenderWidgetHostViewGtk::GetRootWindowBounds() {
  GtkWidget* toplevel = gtk_widget_get_toplevel(view_.get());
  if (!toplevel)
    return gfx::Rect();

  GdkRectangle frame_extents;
  GdkWindow* gdk_window = toplevel->window;
  if (!gdk_window)
    return gfx::Rect();

  gdk_window_get_frame_extents(gdk_window, &frame_extents);
  return gfx::Rect(frame_extents.x, frame_extents.y,
                   frame_extents.width, frame_extents.height);
}

gfx::PluginWindowHandle RenderWidgetHostViewGtk::GetCompositingSurface() {
  if (compositing_surface_ == gfx::kNullPluginWindow) {
    GtkNativeViewManager* manager = GtkNativeViewManager::GetInstance();
    gfx::NativeViewId view_id = GetNativeViewId();

    if (!manager->GetPermanentXIDForId(&compositing_surface_, view_id)) {
      DLOG(ERROR) << "Can't find XID for view id " << view_id;
    }
  }
  return compositing_surface_;
}

bool RenderWidgetHostViewGtk::LockMouse() {
  if (mouse_locked_)
    return true;

  mouse_locked_ = true;

  // Release any current grab.
  GtkWidget* current_grab_window = gtk_grab_get_current();
  if (current_grab_window) {
    gtk_grab_remove(current_grab_window);
    LOG(WARNING) << "Locking Mouse with gdk_pointer_grab, "
                 << "but had to steal grab from another window";
  }

  GtkWidget* widget = view_.get();
  GdkWindow* window = gtk_widget_get_window(widget);
  GdkCursor* cursor = gdk_cursor_new(GDK_BLANK_CURSOR);

  GdkGrabStatus grab_status =
      gdk_pointer_grab(window,
                       FALSE,  // owner_events
                       static_cast<GdkEventMask>(
                           GDK_POINTER_MOTION_MASK |
                           GDK_BUTTON_PRESS_MASK |
                           GDK_BUTTON_RELEASE_MASK),
                       window,  // confine_to
                       cursor,
                       GDK_CURRENT_TIME);

  if (grab_status != GDK_GRAB_SUCCESS) {
    LOG(WARNING) << "Failed to grab pointer for LockMouse. "
                 << "gdk_pointer_grab returned: " << grab_status;
    mouse_locked_ = false;
    return false;
  }

  // Clear the tooltip window.
  SetTooltipText(string16());

  return true;
}

void RenderWidgetHostViewGtk::UnlockMouse() {
  if (!mouse_locked_)
    return;

  mouse_locked_ = false;

  GtkWidget* widget = view_.get();
  GdkDisplay* display = gtk_widget_get_display(widget);
  GdkScreen* screen = gtk_widget_get_screen(widget);
  gdk_display_pointer_ungrab(display, GDK_CURRENT_TIME);
  gdk_display_warp_pointer(display, screen,
                           unlocked_global_mouse_position_.x(),
                           unlocked_global_mouse_position_.y());

  if (host_)
    host_->LostMouseLock();
}

void RenderWidgetHostViewGtk::ForwardKeyboardEvent(
    const NativeWebKeyboardEvent& event) {
  if (!host_)
    return;

#if !defined(OS_CHROMEOS)
  EditCommands edit_commands;
  if (!event.skip_in_browser &&
      key_bindings_handler_->Match(event, &edit_commands)) {
    host_->Send(new ViewMsg_SetEditCommandsForNextKeyEvent(
        host_->routing_id(), edit_commands));
    NativeWebKeyboardEvent copy_event(event);
    copy_event.match_edit_command = true;
    host_->ForwardKeyboardEvent(copy_event);
    return;
  }
#endif

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

void RenderWidgetHostViewGtk::set_last_mouse_down(GdkEventButton* event) {
  GdkEventButton* temp = NULL;
  if (event) {
    temp = reinterpret_cast<GdkEventButton*>(
        gdk_event_copy(reinterpret_cast<GdkEvent*>(event)));
  }

  if (last_mouse_down_)
    gdk_event_free(reinterpret_cast<GdkEvent*>(last_mouse_down_));

  last_mouse_down_ = temp;
}

gfx::Point RenderWidgetHostViewGtk::GetWidgetCenter() {
  if (widget_center_valid_)
    return widget_center_;

  GdkWindow* window = gtk_widget_get_window(view_.get());
  gint window_x = 0;
  gint window_y = 0;
  gdk_window_get_origin(window, &window_x, &window_y);
  gint window_w = 0;
  gint window_h = 0;
  gdk_drawable_get_size(window, &window_w, &window_h);

  widget_center_.SetPoint(window_x + window_w / 2,
                          window_y + window_h / 2);
  widget_center_valid_ = true;
  return widget_center_;
}

void RenderWidgetHostViewGtk::ModifyEventMovementAndCoords(
    WebKit::WebMouseEvent* event) {
  // Movement is computed by taking the difference of the new cursor position
  // and the previous. Under mouse lock the cursor will be warped back to the
  // center so that we are not limited by clipping boundaries.
  // We do not measure movement as the delta from cursor to center because
  // we may receive more mouse movement events before our warp has taken
  // effect.
  event->movementX = event->globalX - global_mouse_position_.x();
  event->movementY = event->globalY - global_mouse_position_.y();

  global_mouse_position_.SetPoint(event->globalX, event->globalY);

  // Under mouse lock, coordinates of mouse are locked to what they were when
  // mouse lock was entered.
  if (mouse_locked_) {
    event->x = unlocked_mouse_position_.x();
    event->y = unlocked_mouse_position_.y();
    event->windowX = unlocked_mouse_position_.x();
    event->windowY = unlocked_mouse_position_.y();
    event->globalX = unlocked_global_mouse_position_.x();
    event->globalY = unlocked_global_mouse_position_.y();
  } else {
    unlocked_mouse_position_.SetPoint(event->windowX, event->windowY);
    unlocked_global_mouse_position_.SetPoint(event->globalX, event->globalY);
  }
}

// static
void RenderWidgetHostView::GetDefaultScreenInfo(
    WebKit::WebScreenInfo* results) {
  GdkWindow* gdk_window =
      gdk_display_get_default_group(gdk_display_get_default());
  content::GetScreenInfoFromNativeWindow(gdk_window, results);
}
