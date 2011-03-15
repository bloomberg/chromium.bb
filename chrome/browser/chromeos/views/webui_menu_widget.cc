// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/views/webui_menu_widget.h"

#include <algorithm>

#include "base/stringprintf.h"
#include "base/singleton.h"
#include "base/task.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/views/menu_locator.h"
#include "chrome/browser/chromeos/views/native_menu_webui.h"
#include "chrome/browser/chromeos/wm_ipc.h"
#include "chrome/browser/ui/views/dom_view.h"
#include "chrome/common/url_constants.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/renderer_host/render_widget_host_view.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "googleurl/src/gurl.h"
#include "third_party/cros/chromeos_wm_ipc_enums.h"
#include "third_party/skia/include/effects/SkGradientShader.h"
#include "ui/gfx/canvas_skia.h"
#include "views/border.h"
#include "views/layout/layout_manager.h"
#include "views/widget/root_view.h"

namespace {

// Colors for the menu's gradient background.
const SkColor kMenuStartColor = SK_ColorWHITE;
const SkColor kMenuEndColor = 0xFFEEEEEE;

// Rounded border for menu. This draws three types of rounded border,
// for context menu, dropdown menu and submenu. Please see
// menu_locator.cc for details.
class RoundedBorder : public views::Border {
 public:
  explicit RoundedBorder(chromeos::MenuLocator* locator)
      : menu_locator_(locator) {
  }

 private:
  // views::Border implementations.
  virtual void Paint(const views::View& view, gfx::Canvas* canvas) const {
    const SkScalar* corners = menu_locator_->GetCorners();
    // The menu is in off screen so no need to draw corners.
    if (!corners)
      return;
    int w = view.width();
    int h = view.height();
    SkRect rect = {0, 0, w, h};
    SkPath path;
    path.addRoundRect(rect, corners);
    SkPaint paint;
    paint.setStyle(SkPaint::kFill_Style);
    paint.setFlags(SkPaint::kAntiAlias_Flag);
    SkPoint p[2] = { {0, 0}, {0, h} };
    SkColor colors[2] = {kMenuStartColor, kMenuEndColor};
    SkShader* s = SkGradientShader::CreateLinear(
        p, colors, NULL, 2, SkShader::kClamp_TileMode, NULL);
    paint.setShader(s);
    // Need to unref shader, otherwise never deleted.
    s->unref();
    canvas->AsCanvasSkia()->drawPath(path, paint);
  }

  virtual void GetInsets(gfx::Insets* insets) const {
    DCHECK(insets);
    menu_locator_->GetInsets(insets);
  }

  chromeos::MenuLocator* menu_locator_;  // not owned

  DISALLOW_COPY_AND_ASSIGN(RoundedBorder);
};

class InsetsLayout : public views::LayoutManager {
 public:
  InsetsLayout() : views::LayoutManager() {}

 private:
  // views::LayoutManager implementations.
  virtual void Layout(views::View* host) {
    if (!host->has_children())
      return;
    gfx::Insets insets = host->GetInsets();
    views::View* view = host->GetChildViewAt(0);

    view->SetBounds(insets.left(), insets.top(),
                    host->width() - insets.width(),
                    host->height() - insets.height());
  }

  virtual gfx::Size GetPreferredSize(views::View* host) {
    DCHECK(host->child_count() == 1);
    gfx::Insets insets = host->GetInsets();
    gfx::Size size = host->GetChildViewAt(0)->GetPreferredSize();
    return gfx::Size(size.width() + insets.width(),
                     size.height() + insets.height());
  }

  DISALLOW_COPY_AND_ASSIGN(InsetsLayout);
};

// A gtk widget key used to test if a given WidgetGtk instance is
// a WebUIMenuWidgetKey.
const char* kWebUIMenuWidgetKey = "__WEBUI_MENU_WIDGET__";

}  // namespace

namespace chromeos {

// static
WebUIMenuWidget* WebUIMenuWidget::FindWebUIMenuWidget(gfx::NativeView native) {
  DCHECK(native);
  native = gtk_widget_get_toplevel(native);
  if (!native)
    return NULL;
  return static_cast<chromeos::WebUIMenuWidget*>(
      g_object_get_data(G_OBJECT(native), kWebUIMenuWidgetKey));
}

///////////////////////////////////////////////////////////////////////////////
// WebUIMenuWidget public:

WebUIMenuWidget::WebUIMenuWidget(chromeos::NativeMenuWebUI* webui_menu,
                                 bool root)
    : views::WidgetGtk(views::WidgetGtk::TYPE_POPUP),
      webui_menu_(webui_menu),
      dom_view_(NULL),
      did_input_grab_(false),
      is_root_(root) {
  DCHECK(webui_menu_);
  // TODO(oshima): Disabling transparent until we migrate bookmark
  // menus to WebUI.  See crosbug.com/7718.
  // MakeTransparent();
}

WebUIMenuWidget::~WebUIMenuWidget() {
}

void WebUIMenuWidget::Init(gfx::NativeView parent, const gfx::Rect& bounds) {
  WidgetGtk::Init(parent, bounds);
  gtk_window_set_destroy_with_parent(GTK_WINDOW(GetNativeView()), TRUE);
  gtk_window_set_type_hint(GTK_WINDOW(GetNativeView()),
                           GDK_WINDOW_TYPE_HINT_MENU);
  g_object_set_data(G_OBJECT(GetNativeView()), kWebUIMenuWidgetKey, this);
}

void WebUIMenuWidget::Hide() {
  ReleaseNativeCapture();
  WidgetGtk::Hide();
  // Clears the content.
  ExecuteJavascript(L"updateModel({'items':[]})");
}

void WebUIMenuWidget::Close() {
  if (dom_view_ != NULL) {
    dom_view_->parent()->RemoveChildView(dom_view_);
    delete dom_view_;
    dom_view_ = NULL;
  }

  // Detach the webui_menu_ which is being deleted.
  webui_menu_ = NULL;
  views::WidgetGtk::Close();
}

void WebUIMenuWidget::ReleaseNativeCapture() {
  WidgetGtk::ReleaseNativeCapture();
  if (did_input_grab_) {
    did_input_grab_ = false;
    gdk_pointer_ungrab(GDK_CURRENT_TIME);
    gdk_keyboard_ungrab(GDK_CURRENT_TIME);

    ClearGrabWidget();
  }
}

gboolean WebUIMenuWidget::OnGrabBrokeEvent(GtkWidget* widget,
                                           GdkEvent* event) {
  did_input_grab_ = false;
  Hide();
  return WidgetGtk::OnGrabBrokeEvent(widget, event);
}

void WebUIMenuWidget::OnSizeAllocate(GtkWidget* widget,
                                     GtkAllocation* allocation) {
  views::WidgetGtk::OnSizeAllocate(widget, allocation);
  // Adjust location when menu gets resized.
  gfx::Rect bounds = GetClientAreaScreenBounds();
  // Don't move until the menu gets contents.
  if (bounds.height() > 1) {
    menu_locator_->Move(this);
    webui_menu_->InputIsReady();
  }
}

gboolean MapToFocus(GtkWidget* widget, GdkEvent* event, gpointer data) {
  WebUIMenuWidget* menu_widget = WebUIMenuWidget::FindWebUIMenuWidget(widget);
  if (menu_widget) {
    // See EnableInput for the meaning of data.
    bool select_item = data != NULL;
    menu_widget->EnableInput(select_item);
  }
  return true;
}

void WebUIMenuWidget::EnableScroll(bool enable) {
  ExecuteJavascript(StringPrintf(
      L"enableScroll(%ls)", enable ? L"true" : L"false"));
}

void WebUIMenuWidget::EnableInput(bool select_item) {
  if (!dom_view_)
    return;
  DCHECK(dom_view_->tab_contents()->render_view_host());
  DCHECK(dom_view_->tab_contents()->render_view_host()->view());
  GtkWidget* target =
      dom_view_->tab_contents()->render_view_host()->view()->GetNativeView();
  DCHECK(target);
  // Skip if the widget already own the input.
  if (gtk_grab_get_current() == target)
    return;

  ClearGrabWidget();

  if (!GTK_WIDGET_REALIZED(target)) {
    // Wait grabbing widget if the widget is not yet realized.
    // Using data as a flag. |select_item| is false if data is NULL,
    // or true otherwise.
    g_signal_connect(G_OBJECT(target), "map-event",
                     G_CALLBACK(&MapToFocus),
                     select_item ? this : NULL);
    return;
  }

  gtk_grab_add(target);
  dom_view_->tab_contents()->Focus();
  if (select_item) {
    ExecuteJavascript(L"selectItem()");
  }
}

void WebUIMenuWidget::ExecuteJavascript(const std::wstring& script) {
  // Don't execute if there is no DOMView associated. This is fine because
  // 1) selectItem make sense only when DOMView is associated.
  // 2) updateModel will be called again when a DOMView is created/assigned.
  if (!dom_view_)
    return;

  DCHECK(dom_view_->tab_contents()->render_view_host());
  dom_view_->tab_contents()->render_view_host()->
      ExecuteJavascriptInWebFrame(string16(), WideToUTF16Hack(script));
}

void WebUIMenuWidget::ShowAt(chromeos::MenuLocator* locator) {
  DCHECK(webui_menu_);
  menu_locator_.reset(locator);
  if (!dom_view_) {
    // TODO(oshima): Replace DOMView with direct use of RVH for beta.
    // DOMView should be refactored to use RVH directly, but
    // it'll require a lot of change and will take time.
    dom_view_ = new DOMView();
    dom_view_->Init(webui_menu_->GetProfile(), NULL);
    // TODO(oshima): remove extra view to draw rounded corner.
    views::View* container = new views::View();
    container->AddChildView(dom_view_);
    container->set_border(new RoundedBorder(locator));
    container->SetLayoutManager(new InsetsLayout());
    SetContentsView(container);
    dom_view_->LoadURL(webui_menu_->menu_url());
  } else {
    webui_menu_->UpdateStates();
    dom_view_->parent()->set_border(new RoundedBorder(locator));
    menu_locator_->Move(this);
  }
  Show();

  // The pointer grab is captured only on the top level menu,
  // all mouse event events are delivered to submenu using gtk_add_grab.
  if (is_root_) {
    CaptureGrab();
  }
}

void WebUIMenuWidget::SetSize(const gfx::Size& new_size) {
  DCHECK(webui_menu_);
  // Ignore the empty new_size request which is called when
  // menu.html is loaded.
  if (new_size.IsEmpty())
    return;
  int width, height;
  gtk_widget_get_size_request(GetNativeView(), &width, &height);
  gfx::Size real_size(std::max(new_size.width(), width),
                      new_size.height());
  // Ignore the size request with the same size.
  gfx::Rect bounds = GetClientAreaScreenBounds();
  if (bounds.size() == real_size)
    return;
  menu_locator_->SetBounds(this, real_size);
}

///////////////////////////////////////////////////////////////////////////////
// WebUIMenuWidget private:

void WebUIMenuWidget::CaptureGrab() {
  // Release the current grab.
  ClearGrabWidget();

  // NOTE: we do this to ensure we get mouse/keyboard events from
  // other apps, a grab done with gtk_grab_add doesn't get events from
  // other apps.
  GdkGrabStatus pointer_grab_status =
      gdk_pointer_grab(window_contents()->window, FALSE,
                       static_cast<GdkEventMask>(
                           GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK |
                           GDK_POINTER_MOTION_MASK),
                       NULL, NULL, GDK_CURRENT_TIME);
  GdkGrabStatus keyboard_grab_status =
      gdk_keyboard_grab(window_contents()->window, FALSE,
                        GDK_CURRENT_TIME);

  did_input_grab_ = pointer_grab_status == GDK_GRAB_SUCCESS &&
      keyboard_grab_status == GDK_GRAB_SUCCESS;
  DCHECK(did_input_grab_);

  EnableInput(false /* no selection */);
}

void WebUIMenuWidget::ClearGrabWidget() {
  GtkWidget* grab_widget;
  while ((grab_widget = gtk_grab_get_current()))
    gtk_grab_remove(grab_widget);
}

}   // namespace chromeos
