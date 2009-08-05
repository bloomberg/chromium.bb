// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab_contents/tab_contents_view_gtk.h"

#include <gdk/gdk.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

#include "base/mime_util.h"
#include "base/gfx/point.h"
#include "base/gfx/rect.h"
#include "base/gfx/size.h"
#include "base/pickle.h"
#include "base/string_util.h"
#include "build/build_config.h"
#include "chrome/browser/download/download_shelf.h"
#include "chrome/browser/gtk/blocked_popup_container_view_gtk.h"
#include "chrome/browser/gtk/browser_window_gtk.h"
#include "chrome/browser/gtk/constrained_window_gtk.h"
#include "chrome/browser/gtk/gtk_dnd_util.h"
#include "chrome/browser/gtk/gtk_floating_container.h"
#include "chrome/browser/gtk/gtk_theme_provider.h"
#include "chrome/browser/gtk/sad_tab_gtk.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/renderer_host/render_view_host_factory.h"
#include "chrome/browser/renderer_host/render_widget_host_view_gtk.h"
#include "chrome/browser/tab_contents/interstitial_page.h"
#include "chrome/browser/tab_contents/render_view_context_menu_gtk.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/tab_contents/tab_contents_delegate.h"
#include "chrome/common/gtk_util.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_source.h"
#include "chrome/common/notification_type.h"
#include "webkit/glue/webdropdata.h"

namespace {

// TODO(erg): I have no idea how to programatically figure out how wide the
// vertical scrollbar is. Hack it with a hardcoded value for now.
const int kScrollbarWidthHack = 25;

// Called when the content view gtk widget is tabbed to, or after the call to
// gtk_widget_child_focus() in TakeFocus(). We return true
// and grab focus if we don't have it. The call to
// FocusThroughTabTraversal(bool) forwards the "move focus forward" effect to
// webkit.
gboolean OnFocus(GtkWidget* widget, GtkDirectionType focus,
                 TabContents* tab_contents) {
  // If we already have focus, let the next widget have a shot at it. We will
  // reach this situation after the call to gtk_widget_child_focus() in
  // TakeFocus().
  if (gtk_widget_is_focus(widget))
    return FALSE;

  gtk_widget_grab_focus(widget);
  bool reverse = focus == GTK_DIR_TAB_BACKWARD;
  tab_contents->FocusThroughTabTraversal(reverse);
  return TRUE;
}

// Called when the mouse leaves the widget. We notify our delegate.
gboolean OnLeaveNotify(GtkWidget* widget, GdkEventCrossing* event,
                       TabContents* tab_contents) {
  if (tab_contents->delegate())
    tab_contents->delegate()->ContentsMouseEvent(tab_contents, false);
  return FALSE;
}

// Called when the mouse moves within the widget. We notify our delegate.
gboolean OnMouseMove(GtkWidget* widget, GdkEventMotion* event,
                     TabContents* tab_contents) {
  if (tab_contents->delegate())
    tab_contents->delegate()->ContentsMouseEvent(tab_contents, true);
  return FALSE;
}

// See tab_contents_view_win.cc for discussion of mouse scroll zooming.
gboolean OnMouseScroll(GtkWidget* widget, GdkEventScroll* event,
                       TabContents* tab_contents) {
  if ((event->state & gtk_accelerator_get_default_mod_mask()) ==
      GDK_CONTROL_MASK) {
    if (event->direction == GDK_SCROLL_DOWN) {
      tab_contents->delegate()->ContentsZoomChange(false);
      return TRUE;
    } else if (event->direction == GDK_SCROLL_UP) {
      tab_contents->delegate()->ContentsZoomChange(true);
      return TRUE;
    }
  }

  return FALSE;
}

// Used with gtk_container_foreach to change the sizes of the children of
// |fixed_|.
void SetSizeRequest(GtkWidget* widget, gpointer userdata) {
  gfx::Size* size = static_cast<gfx::Size*>(userdata);
  if (widget->allocation.width != size->width() ||
      widget->allocation.height != size->height()) {
    gtk_widget_set_size_request(widget, size->width(), size->height());
  }
}

int GdkEventKeyToLayoutIndependentKeyval(const GdkEventKey* event) {
  // This function is a copy of WebKit::gdkEventToWindowsKeyCode() but returns
  // a GDK key value instead of a Windows virtual key code.
  static const int kHardwareCodeToGdkKeyval[] = {
    0,                 // 0x00:
    0,                 // 0x01:
    0,                 // 0x02:
    0,                 // 0x03:
    0,                 // 0x04:
    0,                 // 0x05:
    0,                 // 0x06:
    0,                 // 0x07:
    0,                 // 0x08:
    0,                 // 0x09: GDK_Escape
    GDK_1,             // 0x0A: GDK_1
    GDK_2,             // 0x0B: GDK_2
    GDK_3,             // 0x0C: GDK_3
    GDK_4,             // 0x0D: GDK_4
    GDK_5,             // 0x0E: GDK_5
    GDK_6,             // 0x0F: GDK_6
    GDK_7,             // 0x10: GDK_7
    GDK_8,             // 0x11: GDK_8
    GDK_9,             // 0x12: GDK_9
    GDK_0,             // 0x13: GDK_0
    GDK_minus,         // 0x14: GDK_minus
    GDK_equal,         // 0x15: GDK_equal
    0,                 // 0x16: GDK_BackSpace
    0,                 // 0x17: GDK_Tab
    GDK_q,             // 0x18: GDK_q
    GDK_w,             // 0x19: GDK_w
    GDK_e,             // 0x1A: GDK_e
    GDK_r,             // 0x1B: GDK_r
    GDK_t,             // 0x1C: GDK_t
    GDK_y,             // 0x1D: GDK_y
    GDK_u,             // 0x1E: GDK_u
    GDK_i,             // 0x1F: GDK_i
    GDK_o,             // 0x20: GDK_o
    GDK_p,             // 0x21: GDK_p
    GDK_bracketleft,   // 0x22: GDK_bracketleft
    GDK_bracketright,  // 0x23: GDK_bracketright
    0,                 // 0x24: GDK_Return
    0,                 // 0x25: GDK_Control_L
    GDK_a,             // 0x26: GDK_a
    GDK_s,             // 0x27: GDK_s
    GDK_d,             // 0x28: GDK_d
    GDK_f,             // 0x29: GDK_f
    GDK_g,             // 0x2A: GDK_g
    GDK_h,             // 0x2B: GDK_h
    GDK_j,             // 0x2C: GDK_j
    GDK_k,             // 0x2D: GDK_k
    GDK_l,             // 0x2E: GDK_l
    GDK_semicolon,     // 0x2F: GDK_semicolon
    GDK_apostrophe,    // 0x30: GDK_apostrophe
    GDK_grave,         // 0x31: GDK_grave
    0,                 // 0x32: GDK_Shift_L
    GDK_backslash,     // 0x33: GDK_backslash
    GDK_z,             // 0x34: GDK_z
    GDK_x,             // 0x35: GDK_x
    GDK_c,             // 0x36: GDK_c
    GDK_v,             // 0x37: GDK_v
    GDK_b,             // 0x38: GDK_b
    GDK_n,             // 0x39: GDK_n
    GDK_m,             // 0x3A: GDK_m
    GDK_comma,         // 0x3B: GDK_comma
    GDK_period,        // 0x3C: GDK_period
    GDK_slash,         // 0x3D: GDK_slash
    0,                 // 0x3E: GDK_Shift_R
  };

  // If this |event->keyval| represents an ASCII character, we use it for
  // accelerators. Otherwise, we look up a table from a hardware keycode to
  // keyval and use its result.
  wchar_t c = gdk_keyval_to_unicode(event->keyval);
  if (c <= 0x7F)
    return event->keyval;

  if (event->hardware_keycode < arraysize(kHardwareCodeToGdkKeyval)) {
    int keyval = kHardwareCodeToGdkKeyval[event->hardware_keycode];
    if (keyval)
      return keyval;
  }

  return event->keyval;
}

// Get the current location of the mouse cursor relative to the screen.
gfx::Point ScreenPoint(GtkWidget* widget) {
  int x, y;
  gdk_display_get_pointer(gtk_widget_get_display(widget), NULL, &x, &y,
                          NULL);
  return gfx::Point(x, y);
}

// Get the current location of the mouse cursor relative to the widget.
gfx::Point ClientPoint(GtkWidget* widget) {
  int x, y;
  gtk_widget_get_pointer(widget, &x, &y);
  return gfx::Point(x, y);
}

}  // namespace

// A helper class that handles DnD for drops in the renderer. In GTK parlance,
// this handles destination-side DnD, but not source-side DnD.
class WebDragDest {
 public:
  explicit WebDragDest(TabContents* tab_contents, GtkWidget* widget)
      : tab_contents_(tab_contents),
        widget_(widget),
        context_(NULL),
        method_factory_(this) {
    gtk_drag_dest_set(widget, static_cast<GtkDestDefaults>(0),
                      NULL, 0, GDK_ACTION_COPY);
    g_signal_connect(widget, "drag-motion",
                     G_CALLBACK(OnDragMotionThunk), this);
    g_signal_connect(widget, "drag-leave",
                     G_CALLBACK(OnDragLeaveThunk), this);
    g_signal_connect(widget, "drag-drop",
                     G_CALLBACK(OnDragDropThunk), this);
    g_signal_connect(widget, "drag-data-received",
                     G_CALLBACK(OnDragDataReceivedThunk), this);

    destroy_handler_ = g_signal_connect(widget, "destroy",
        G_CALLBACK(gtk_widget_destroyed), &widget_);
  }

  ~WebDragDest() {
    if (widget_) {
      gtk_drag_dest_unset(widget_);
      g_signal_handler_disconnect(widget_, destroy_handler_);
    }
  }

  // This is called when the renderer responds to a drag motion event. We must
  // update the system drag cursor.
  void UpdateDragStatus(bool is_drop_target) {
    if (context_) {
      // TODO(estade): we might want to support other actions besides copy,
      // but that would increase the cost of getting our drag success guess
      // wrong.
      gdk_drag_status(context_, is_drop_target ? GDK_ACTION_COPY :
                                static_cast<GdkDragAction>(0),
                      drag_over_time_);
      is_drop_target_ = false;
    }
  }

  // Informs the renderer when a system drag has left the render view.
  // See OnDragLeave().
  void DragLeave() {
    tab_contents_->render_view_host()->DragTargetDragLeave();
  }

 private:
  static gboolean OnDragMotionThunk(GtkWidget* widget,
      GdkDragContext* drag_context, gint x, gint y, guint time,
      WebDragDest* dest) {
    return dest->OnDragMotion(drag_context, x, y, time);
  }
  static void OnDragLeaveThunk(GtkWidget* widget,
      GdkDragContext* drag_context, guint time, WebDragDest* dest) {
    dest->OnDragLeave(drag_context, time);
  }
  static gboolean OnDragDropThunk(GtkWidget* widget,
      GdkDragContext* drag_context, gint x, gint y, guint time,
      WebDragDest* dest) {
    return dest->OnDragDrop(drag_context, x, y, time);
  }
  static void OnDragDataReceivedThunk(GtkWidget* widget,
      GdkDragContext* drag_context, gint x, gint y,
      GtkSelectionData* data, guint info, guint time, WebDragDest* dest) {
    dest->OnDragDataReceived(drag_context, x, y, data, info, time);
  }

  // Called when a system drag crosses over the render view. As there is no drag
  // enter event, we treat it as an enter event (and not a regular motion event)
  // when |context_| is NULL.
  gboolean OnDragMotion(GdkDragContext* context, gint x, gint y, guint time) {
    if (context_ != context) {
      context_ = context;
      drop_data_.reset(new WebDropData);
      is_drop_target_ = false;

      static int supported_targets[] = {
        GtkDndUtil::TEXT_PLAIN,
        GtkDndUtil::TEXT_URI_LIST,
        GtkDndUtil::TEXT_HTML,
        // TODO(estade): support image drags?
      };

      data_requests_ = arraysize(supported_targets);
      for (size_t i = 0; i < arraysize(supported_targets); ++i) {
        gtk_drag_get_data(widget_, context,
                          GtkDndUtil::GetAtomForTarget(supported_targets[i]),
                          time);
      }
    } else if (data_requests_ == 0) {
      tab_contents_->render_view_host()->
          DragTargetDragOver(ClientPoint(widget_), ScreenPoint(widget_));
      drag_over_time_ = time;
    }

    // Pretend we are a drag destination because we don't want to wait for
    // the renderer to tell us if we really are or not.
    return TRUE;
  }

  // We make a series of requests for the drag data when the drag first enters
  // the render view. This is the callback that is used to give us the data
  // for each individual target. When |data_requests_| reaches 0, we know we
  // have attained all the data, and we can finally tell the renderer about the
  // drag.
  void OnDragDataReceived(GdkDragContext* context, gint x, gint y,
                          GtkSelectionData* data, guint info, guint time) {
    // We might get the data from an old get_data() request that we no longer
    // care about.
    if (context != context_)
      return;

    data_requests_--;

    // Decode the data.
    if (data->data) {
      // If the source can't provide us with valid data for a requested target,
      // data->data will be NULL.
      if (data->target ==
          GtkDndUtil::GetAtomForTarget(GtkDndUtil::TEXT_PLAIN)) {
        guchar* text = gtk_selection_data_get_text(data);
        if (text) {
          drop_data_->plain_text = UTF8ToUTF16(std::string(
              reinterpret_cast<char*>(text), data->length));
          g_free(text);
        }
      } else if (data->target ==
          GtkDndUtil::GetAtomForTarget(GtkDndUtil::TEXT_URI_LIST)) {
        gchar** uris = gtk_selection_data_get_uris(data);
        if (uris) {
          for (gchar** uri_iter = uris; *uri_iter; uri_iter++) {
            // TODO(estade): Can the filenames have a non-UTF8 encoding?
            drop_data_->filenames.push_back(UTF8ToUTF16(*uri_iter));
          }
          // Also, write the first URI as the URL.
          if (uris[0])
            drop_data_->url = GURL(uris[0]);
          g_strfreev(uris);
        }
      } else if (data->target ==
          GtkDndUtil::GetAtomForTarget(GtkDndUtil::TEXT_HTML)) {
        // TODO(estade): Can the html have a non-UTF8 encoding?
        drop_data_->text_html = UTF8ToUTF16(std::string(
            reinterpret_cast<char*>(data->data), data->length));
        // We leave the base URL empty.
      }
    }

    if (data_requests_ == 0) {
      // Tell the renderer about the drag.
      // |x| and |y| are seemingly arbitrary at this point.
      tab_contents_->render_view_host()->
          DragTargetDragEnter(*drop_data_.get(),
                              ClientPoint(widget_), ScreenPoint(widget_));
      drag_over_time_ = time;
    }
  }

  // The drag has left our widget; forward this information to the renderer.
  void OnDragLeave(GdkDragContext* context, guint time) {
    // Set |context_| to NULL to make sure we will recognize the next DragMotion
    // as an enter.
    context_ = NULL;
    drop_data_.reset();
    // When GTK sends us a drag-drop signal, it is shortly (and synchronously)
    // preceded by a drag-leave. The renderer doesn't like getting the signals
    // in this order so delay telling it about the drag-leave till we are sure
    // we are not getting a drop as well.
    MessageLoop::current()->PostTask(FROM_HERE,
        method_factory_.NewRunnableMethod(&WebDragDest::DragLeave));
  }

  // Called by GTK when the user releases the mouse, executing a drop.
  gboolean OnDragDrop(GdkDragContext* context, gint x, gint y, guint time) {
    // Cancel that drag leave!
    method_factory_.RevokeAll();

    tab_contents_->render_view_host()->
        DragTargetDrop(ClientPoint(widget_), ScreenPoint(widget_));

    // The second parameter is just an educated guess, but at least we will
    // get the drag-end animation right sometimes.
    gtk_drag_finish(context, is_drop_target_, FALSE, time);
    return TRUE;
  }


  TabContents* tab_contents_;
  // The render view.
  GtkWidget* widget_;
  // The current drag context for system drags over our render view, or NULL if
  // there is no system drag or the system drag is not over our render view.
  GdkDragContext* context_;
  // The data for the current drag, or NULL if |context_| is NULL.
  scoped_ptr<WebDropData> drop_data_;

  // The number of outstanding drag data requests we have sent to the drag
  // source.
  int data_requests_;

  // The last time we sent a message to the renderer related to a drag motion.
  gint drag_over_time_;

  // Whether the cursor is over a drop target, according to the last message we
  // got from the renderer.
  bool is_drop_target_;

  // Handler ID for the destroy signal handler. We connect to the destroy
  // signal handler so that we won't call dest_unset on it after it is
  // destroyed, but we have to cancel the handler if we are destroyed before
  // |widget_| is.
  int destroy_handler_;

  ScopedRunnableMethodFactory<WebDragDest> method_factory_;
  DISALLOW_COPY_AND_ASSIGN(WebDragDest);
};

// static
TabContentsView* TabContentsView::Create(TabContents* tab_contents) {
  return new TabContentsViewGtk(tab_contents);
}

TabContentsViewGtk::TabContentsViewGtk(TabContents* tab_contents)
    : TabContentsView(tab_contents),
      floating_(gtk_floating_container_new()),
      fixed_(gtk_fixed_new()),
      popup_view_(NULL),
      drag_failed_(false),
      drag_widget_(NULL) {
  g_signal_connect(fixed_, "size-allocate",
                   G_CALLBACK(OnSizeAllocate), this);
  g_signal_connect(floating_.get(), "set-floating-position",
                   G_CALLBACK(OnSetFloatingPosition), this);

  gtk_container_add(GTK_CONTAINER(floating_.get()), fixed_);
  gtk_widget_show(fixed_);
  gtk_widget_show(floating_.get());
  registrar_.Add(this, NotificationType::TAB_CONTENTS_CONNECTED,
                 Source<TabContents>(tab_contents));

  // Renderer source DnD.
  drag_widget_ = gtk_invisible_new();
  g_signal_connect(drag_widget_, "drag-failed",
                   G_CALLBACK(OnDragFailedThunk), this);
  g_signal_connect(drag_widget_, "drag-end", G_CALLBACK(OnDragEndThunk), this);
  g_signal_connect(drag_widget_, "drag-data-get",
                   G_CALLBACK(OnDragDataGetThunk), this);
  g_object_ref_sink(drag_widget_);
}

TabContentsViewGtk::~TabContentsViewGtk() {
  floating_.Destroy();
}

void TabContentsViewGtk::AttachBlockedPopupView(
    BlockedPopupContainerViewGtk* popup_view) {
  DCHECK(popup_view_ == NULL);
  popup_view_ = popup_view;
  gtk_floating_container_add_floating(GTK_FLOATING_CONTAINER(floating_.get()),
                                      popup_view->widget());
}

void TabContentsViewGtk::RemoveBlockedPopupView(
    BlockedPopupContainerViewGtk* popup_view) {
  DCHECK(popup_view_ == popup_view);
  gtk_container_remove(GTK_CONTAINER(floating_.get()), popup_view->widget());
  popup_view_ = NULL;
}

void TabContentsViewGtk::AttachConstrainedWindow(
    ConstrainedWindowGtk* constrained_window) {
  DCHECK(find(constrained_windows_.begin(), constrained_windows_.end(),
              constrained_window) == constrained_windows_.end());

  constrained_windows_.push_back(constrained_window);
  gtk_floating_container_add_floating(GTK_FLOATING_CONTAINER(floating_.get()),
                                      constrained_window->widget());
}

void TabContentsViewGtk::RemoveConstrainedWindow(
    ConstrainedWindowGtk* constrained_window) {
  std::vector<ConstrainedWindowGtk*>::iterator item =
      find(constrained_windows_.begin(), constrained_windows_.end(),
           constrained_window);
  DCHECK(item != constrained_windows_.end());

  gtk_container_remove(GTK_CONTAINER(floating_.get()),
                       constrained_window->widget());
  constrained_windows_.erase(item);
}

void TabContentsViewGtk::CreateView() {
  // Windows uses this to do initialization, but we do all our initialization
  // in the constructor.
}

RenderWidgetHostView* TabContentsViewGtk::CreateViewForWidget(
    RenderWidgetHost* render_widget_host) {
  if (render_widget_host->view()) {
    // During testing, the view will already be set up in most cases to the
    // test view, so we don't want to clobber it with a real one. To verify that
    // this actually is happening (and somebody isn't accidentally creating the
    // view twice), we check for the RVH Factory, which will be set when we're
    // making special ones (which go along with the special views).
    DCHECK(RenderViewHostFactory::has_factory());
    return render_widget_host->view();
  }

  RenderWidgetHostViewGtk* view =
      new RenderWidgetHostViewGtk(render_widget_host);
  view->InitAsChild();
  gfx::NativeView content_view = view->native_view();
  g_signal_connect(content_view, "focus",
                   G_CALLBACK(OnFocus), tab_contents());
  g_signal_connect(content_view, "leave-notify-event",
                   G_CALLBACK(OnLeaveNotify), tab_contents());
  g_signal_connect(content_view, "motion-notify-event",
                   G_CALLBACK(OnMouseMove), tab_contents());
  g_signal_connect(content_view, "scroll-event",
                   G_CALLBACK(OnMouseScroll), tab_contents());
  gtk_widget_add_events(content_view, GDK_LEAVE_NOTIFY_MASK |
                        GDK_POINTER_MOTION_MASK);
  g_signal_connect(content_view, "button-press-event",
                   G_CALLBACK(OnMouseDown), this);
  InsertIntoContentArea(content_view);

  // Renderer target DnD.
  drag_dest_.reset(new WebDragDest(tab_contents(), content_view));

  return view;
}

gfx::NativeView TabContentsViewGtk::GetNativeView() const {
  return floating_.get();
}

gfx::NativeView TabContentsViewGtk::GetContentNativeView() const {
  if (!tab_contents()->render_widget_host_view())
    return NULL;
  return tab_contents()->render_widget_host_view()->GetNativeView();
}

gfx::NativeWindow TabContentsViewGtk::GetTopLevelNativeWindow() const {
  GtkWidget* window = gtk_widget_get_ancestor(GetNativeView(), GTK_TYPE_WINDOW);
  return window ? GTK_WINDOW(window) : NULL;
}

void TabContentsViewGtk::InitRendererPrefs(RendererPreferences* prefs) {
  GtkSettings* gtk_settings = gtk_settings_get_default();

  gint antialias = 0;
  gint hinting = 0;
  gchar* hint_style = NULL;
  gchar* rgba_style = NULL;
  g_object_get(gtk_settings,
               "gtk-xft-antialias", &antialias,
               "gtk-xft-hinting", &hinting,
               "gtk-xft-hintstyle", &hint_style,
               "gtk-xft-rgba", &rgba_style,
               NULL);

  // g_object_get() doesn't tell us whether the properties were present or not,
  // but if they aren't (because gnome-settings-daemon isn't running), we'll get
  // NULL values for the strings.
  if (hint_style && rgba_style) {
    prefs->should_antialias_text = antialias;

    if (hinting == 0 || strcmp(hint_style, "hintnone") == 0) {
      prefs->hinting = RENDERER_PREFERENCES_HINTING_NONE;
    } else if (strcmp(hint_style, "hintslight") == 0) {
      prefs->hinting = RENDERER_PREFERENCES_HINTING_SLIGHT;
    } else if (strcmp(hint_style, "hintmedium") == 0) {
      prefs->hinting = RENDERER_PREFERENCES_HINTING_MEDIUM;
    } else if (strcmp(hint_style, "hintfull") == 0) {
      prefs->hinting = RENDERER_PREFERENCES_HINTING_FULL;
    }

    if (strcmp(rgba_style, "none") == 0) {
      prefs->subpixel_rendering = RENDERER_PREFERENCES_SUBPIXEL_RENDERING_NONE;
    } else if (strcmp(rgba_style, "rgb") == 0) {
      prefs->subpixel_rendering = RENDERER_PREFERENCES_SUBPIXEL_RENDERING_RGB;
    } else if (strcmp(rgba_style, "bgr") == 0) {
      prefs->subpixel_rendering = RENDERER_PREFERENCES_SUBPIXEL_RENDERING_BGR;
    } else if (strcmp(rgba_style, "vrgb") == 0) {
      prefs->subpixel_rendering = RENDERER_PREFERENCES_SUBPIXEL_RENDERING_VRGB;
    } else if (strcmp(rgba_style, "vbgr") == 0) {
      prefs->subpixel_rendering = RENDERER_PREFERENCES_SUBPIXEL_RENDERING_VBGR;
    }
  }

  if (hint_style)
    g_free(hint_style);
  if (rgba_style)
    g_free(rgba_style);
}

void TabContentsViewGtk::GetContainerBounds(gfx::Rect* out) const {
  // This is used for positioning the download shelf arrow animation,
  // as well as sizing some other widgets in Windows.  In GTK the size is
  // managed for us, so it appears to be only used for the download shelf
  // animation.
  int x = 0;
  int y = 0;
  if (fixed_->window)
    gdk_window_get_origin(fixed_->window, &x, &y);
  out->SetRect(x + fixed_->allocation.x, y + fixed_->allocation.y,
               fixed_->allocation.width, fixed_->allocation.height);
}

void TabContentsViewGtk::OnContentsDestroy() {
  // We don't want to try to handle drag events from this point on.
  g_signal_handlers_disconnect_by_func(drag_widget_,
      reinterpret_cast<gpointer>(OnDragFailedThunk), this);
  g_signal_handlers_disconnect_by_func(drag_widget_,
      reinterpret_cast<gpointer>(OnDragEndThunk), this);
  g_signal_handlers_disconnect_by_func(drag_widget_,
      reinterpret_cast<gpointer>(OnDragDataGetThunk), this);

  // Break the current drag, if any.
  if (drop_data_.get()) {
    gtk_grab_add(drag_widget_);
    gtk_grab_remove(drag_widget_);
    MessageLoopForUI::current()->RemoveObserver(this);
    drop_data_.reset();
  }

  gtk_widget_destroy(drag_widget_);
  g_object_unref(drag_widget_);
  drag_widget_ = NULL;
}

void TabContentsViewGtk::SetPageTitle(const std::wstring& title) {
  // Set the window name to include the page title so it's easier to spot
  // when debugging (e.g. via xwininfo -tree).
  gfx::NativeView content_view = GetContentNativeView();
  if (content_view && content_view->window)
    gdk_window_set_title(content_view->window, WideToUTF8(title).c_str());
}

void TabContentsViewGtk::OnTabCrashed() {
  if (!sad_tab_.get()) {
    sad_tab_.reset(new SadTabGtk);
    InsertIntoContentArea(sad_tab_->widget());
    gtk_widget_show(sad_tab_->widget());
  }
}

void TabContentsViewGtk::SizeContents(const gfx::Size& size) {
  // This function is a hack and should go away. In any case we don't manually
  // control the size of the contents on linux, so do nothing.
}

void TabContentsViewGtk::Focus() {
  if (tab_contents()->showing_interstitial_page()) {
    tab_contents()->interstitial_page()->Focus();
  } else {
    GtkWidget* widget = GetContentNativeView();
    if (widget)
      gtk_widget_grab_focus(widget);
  }
}

void TabContentsViewGtk::SetInitialFocus() {
  if (tab_contents()->FocusLocationBarByDefault())
    tab_contents()->delegate()->SetFocusToLocationBar();
  else
    Focus();
}

void TabContentsViewGtk::StoreFocus() {
  focus_store_.Store(GetNativeView());
}

void TabContentsViewGtk::RestoreFocus() {
  if (focus_store_.widget())
    gtk_widget_grab_focus(focus_store_.widget());
  else
    SetInitialFocus();
}

void TabContentsViewGtk::UpdateDragCursor(bool is_drop_target) {
  drag_dest_->UpdateDragStatus(is_drop_target);
}

void TabContentsViewGtk::GotFocus() {
  NOTIMPLEMENTED();
}

// This is called when we the renderer asks us to take focus back (i.e., it has
// iterated past the last focusable element on the page).
void TabContentsViewGtk::TakeFocus(bool reverse) {
  gtk_widget_child_focus(GTK_WIDGET(GetTopLevelNativeWindow()),
      reverse ? GTK_DIR_TAB_BACKWARD : GTK_DIR_TAB_FORWARD);
}

void TabContentsViewGtk::HandleKeyboardEvent(
    const NativeWebKeyboardEvent& event) {
  // This may be an accelerator. Try to pass it on to our browser window
  // to handle.
  GtkWindow* window = GetTopLevelNativeWindow();
  if (!window) {
    NOTREACHED();
    return;
  }

  // Filter out pseudo key events created by GtkIMContext signal handlers.
  // Since GtkIMContext signal handlers don't use GdkEventKey objects, its
  // |event.os_event| values are dummy values (or NULL.)
  // We should filter out these pseudo key events to prevent unexpected
  // behaviors caused by them.
  if (event.type == WebKit::WebInputEvent::Char)
    return;

  BrowserWindowGtk* browser_window =
      BrowserWindowGtk::GetBrowserWindowForNativeWindow(window);
  DCHECK(browser_window);
  browser_window->HandleAccelerator(
      GdkEventKeyToLayoutIndependentKeyval(event.os_event),
      static_cast<GdkModifierType>(event.os_event->state));
}

void TabContentsViewGtk::Observe(NotificationType type,
                                 const NotificationSource& source,
                                 const NotificationDetails& details) {
  switch (type.value) {
    case NotificationType::TAB_CONTENTS_CONNECTED: {
      // No need to remove the SadTabGtk's widget from the container since
      // the new RenderWidgetHostViewGtk instance already removed all the
      // vbox's children.
      sad_tab_.reset();
      break;
    }
    default:
      NOTREACHED() << "Got a notification we didn't register for.";
      break;
  }
}

void TabContentsViewGtk::WillProcessEvent(GdkEvent* event) {
  // No-op.
}

void TabContentsViewGtk::DidProcessEvent(GdkEvent* event) {
  if (event->type != GDK_MOTION_NOTIFY)
    return;

  GdkEventMotion* event_motion = reinterpret_cast<GdkEventMotion*>(event);
  gfx::Point client = ClientPoint(GetContentNativeView());

  if (tab_contents()->render_view_host()) {
    tab_contents()->render_view_host()->DragSourceMovedTo(
        client.x(), client.y(), event_motion->x_root, event_motion->y_root);
  }
}

void TabContentsViewGtk::ShowContextMenu(const ContextMenuParams& params) {
  context_menu_.reset(new RenderViewContextMenuGtk(tab_contents(), params,
                                                   last_mouse_down_.time));
  context_menu_->Init();
  context_menu_->Popup();
}

// Render view DnD -------------------------------------------------------------

void TabContentsViewGtk::StartDragging(const WebDropData& drop_data) {
  DCHECK(GetContentNativeView());

  int targets_mask = 0;

  if (!drop_data.plain_text.empty())
    targets_mask |= GtkDndUtil::TEXT_PLAIN;
  if (drop_data.url.is_valid()) {
    targets_mask |= GtkDndUtil::TEXT_URI_LIST;
    targets_mask |= GtkDndUtil::CHROME_NAMED_URL;
  }
  if (!drop_data.text_html.empty())
    targets_mask |= GtkDndUtil::TEXT_HTML;
  if (!drop_data.file_contents.empty())
    targets_mask |= GtkDndUtil::CHROME_WEBDROP_FILE_CONTENTS;

  if (targets_mask == 0) {
    NOTIMPLEMENTED();
    if (tab_contents()->render_view_host())
      tab_contents()->render_view_host()->DragSourceSystemDragEnded();
  }

  drop_data_.reset(new WebDropData(drop_data));

  GtkTargetList* list = GtkDndUtil::GetTargetListFromCodeMask(targets_mask);
  if (targets_mask & GtkDndUtil::CHROME_WEBDROP_FILE_CONTENTS) {
    drag_file_mime_type_ = gdk_atom_intern(
        mime_util::GetDataMimeType(drop_data.file_contents).c_str(), FALSE);
    gtk_target_list_add(list, drag_file_mime_type_,
                        0, GtkDndUtil::CHROME_WEBDROP_FILE_CONTENTS);
  }

  drag_failed_ = false;
  // If we don't pass an event, GDK won't know what event time to start grabbing
  // mouse events. Technically it's the mouse motion event and not the mouse
  // down event that causes the drag, but there's no reliable way to know
  // *which* motion event initiated the drag, so this will have to do.
  // TODO(estade): This can sometimes be very far off, e.g. if the user clicks
  // and holds and doesn't start dragging for a long time. I doubt it matters
  // much, but we should probably look into the possibility of getting the
  // initiating event from webkit.
  gtk_drag_begin(drag_widget_, list, GDK_ACTION_COPY,
                 1,  // Drags are always initiated by the left button.
                 reinterpret_cast<GdkEvent*>(&last_mouse_down_));
  MessageLoopForUI::current()->AddObserver(this);
  // The drag adds a ref; let it own the list.
  gtk_target_list_unref(list);
}

void TabContentsViewGtk::OnDragDataGet(
    GdkDragContext* context, GtkSelectionData* selection_data,
    guint target_type, guint time) {
  const int bits_per_byte = 8;

  switch (target_type) {
    case GtkDndUtil::TEXT_PLAIN: {
      std::string utf8_text = UTF16ToUTF8(drop_data_->plain_text);
      gtk_selection_data_set_text(selection_data, utf8_text.c_str(),
                                  utf8_text.length());
      break;
    }

    case GtkDndUtil::TEXT_URI_LIST: {
      gchar* uri_array[2];
      uri_array[0] = strdup(drop_data_->url.spec().c_str());
      uri_array[1] = NULL;
      gtk_selection_data_set_uris(selection_data, uri_array);
      free(uri_array[0]);
      break;
    }

    case GtkDndUtil::TEXT_HTML: {
      // TODO(estade): change relative links to be absolute using
      // |html_base_url|.
      std::string utf8_text = UTF16ToUTF8(drop_data_->text_html);
      gtk_selection_data_set(selection_data,
          GtkDndUtil::GetAtomForTarget(GtkDndUtil::TEXT_HTML),
          bits_per_byte,
          reinterpret_cast<const guchar*>(utf8_text.c_str()),
          utf8_text.length());
      break;
    }

    case GtkDndUtil::CHROME_NAMED_URL: {
      Pickle pickle;
      pickle.WriteString(UTF16ToUTF8(drop_data_->url_title));
      pickle.WriteString(drop_data_->url.spec());
      gtk_selection_data_set(selection_data,
          GtkDndUtil::GetAtomForTarget(GtkDndUtil::CHROME_NAMED_URL),
          bits_per_byte,
          reinterpret_cast<const guchar*>(pickle.data()),
          pickle.size());
      break;
    }

    case GtkDndUtil::CHROME_WEBDROP_FILE_CONTENTS: {
      gtk_selection_data_set(selection_data,
          drag_file_mime_type_, bits_per_byte,
          reinterpret_cast<const guchar*>(drop_data_->file_contents.data()),
          drop_data_->file_contents.length());
      break;
    }

    default:
      NOTREACHED();
  }
}

gboolean TabContentsViewGtk::OnDragFailed() {
  drag_failed_ = true;

  gfx::Point root = ScreenPoint(GetContentNativeView());
  gfx::Point client = ClientPoint(GetContentNativeView());

  if (tab_contents()->render_view_host()) {
    tab_contents()->render_view_host()->DragSourceCancelledAt(
        client.x(), client.y(), root.x(), root.y());
  }

  // Let the native failure animation run.
  return FALSE;
}

void TabContentsViewGtk::OnDragEnd() {
  MessageLoopForUI::current()->RemoveObserver(this);

  if (!drag_failed_) {
    gfx::Point root = ScreenPoint(GetContentNativeView());
    gfx::Point client = ClientPoint(GetContentNativeView());

    if (tab_contents()->render_view_host()) {
      tab_contents()->render_view_host()->DragSourceEndedAt(
          client.x(), client.y(), root.x(), root.y());
    }
  }

  if (tab_contents()->render_view_host())
      tab_contents()->render_view_host()->DragSourceSystemDragEnded();

  drop_data_.reset();
}

// -----------------------------------------------------------------------------

void TabContentsViewGtk::InsertIntoContentArea(GtkWidget* widget) {
  gtk_fixed_put(GTK_FIXED(fixed_), widget, 0, 0);
}

gboolean TabContentsViewGtk::OnMouseDown(GtkWidget* widget,
    GdkEventButton* event, TabContentsViewGtk* view) {
  view->last_mouse_down_ = *event;
  return FALSE;
}

gboolean TabContentsViewGtk::OnSizeAllocate(GtkWidget* widget,
                                            GtkAllocation* allocation,
                                            TabContentsViewGtk* view) {
  int width = allocation->width;
  int height = allocation->height;
  // |delegate()| can be NULL here during browser teardown.
  if (view->tab_contents()->delegate())
    height += view->tab_contents()->delegate()->GetExtraRenderViewHeight();
  gfx::Size size(width, height);
  gtk_container_foreach(GTK_CONTAINER(widget), SetSizeRequest, &size);

  return FALSE;
}

// static
void TabContentsViewGtk::OnSetFloatingPosition(
    GtkFloatingContainer* floating_container, GtkAllocation* allocation,
    TabContentsViewGtk* tab_contents_view) {
  if (tab_contents_view->popup_view_) {
    GtkWidget* widget = tab_contents_view->popup_view_->widget();

    // Look at the size request of the status bubble and tell the
    // GtkFloatingContainer where we want it positioned.
    GtkRequisition requisition;
    gtk_widget_size_request(widget, &requisition);

    GValue value = { 0, };
    g_value_init(&value, G_TYPE_INT);

    int child_x = std::max(
        allocation->x + allocation->width - requisition.width -
        kScrollbarWidthHack, 0);
    g_value_set_int(&value, child_x);
    gtk_container_child_set_property(GTK_CONTAINER(floating_container),
                                     widget, "x", &value);

    int child_y = std::max(
        allocation->y + allocation->height - requisition.height, 0);
    g_value_set_int(&value, child_y);
    gtk_container_child_set_property(GTK_CONTAINER(floating_container),
                                     widget, "y", &value);
    g_value_unset(&value);
  }

  // Place each ConstrainedWindow in the center of the view.
  int half_view_width = std::max((allocation->x + allocation->width) / 2, 0);
  int half_view_height = std::max((allocation->y + allocation->height) / 2, 0);
  std::vector<ConstrainedWindowGtk*>::iterator it =
      tab_contents_view->constrained_windows_.begin();
  std::vector<ConstrainedWindowGtk*>::iterator end =
      tab_contents_view->constrained_windows_.end();
  for (; it != end; ++it) {
    GtkWidget* widget = (*it)->widget();
    DCHECK(widget->parent == tab_contents_view->floating_.get());

    GtkRequisition requisition;
    gtk_widget_size_request(widget, &requisition);

    GValue value = { 0, };
    g_value_init(&value, G_TYPE_INT);

    int child_x = std::max(half_view_width - (requisition.width / 2), 0);
    g_value_set_int(&value, child_x);
    gtk_container_child_set_property(GTK_CONTAINER(floating_container),
                                     widget, "x", &value);

    int child_y = std::max(half_view_height - (requisition.height / 2), 0);
    g_value_set_int(&value, child_y);
    gtk_container_child_set_property(GTK_CONTAINER(floating_container),
                                     widget, "y", &value);
    g_value_unset(&value);
  }
}
