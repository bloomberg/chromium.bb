// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tab_contents/native_tab_contents_view_gtk.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/ui/gtk/constrained_window_gtk.h"
#include "chrome/browser/tab_contents/web_drag_bookmark_handler_gtk.h"
#include "chrome/browser/ui/views/tab_contents/native_tab_contents_view_delegate.h"
#include "content/browser/renderer_host/render_widget_host_view_gtk.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/browser/tab_contents/tab_contents_view.h"
#include "content/browser/tab_contents/web_drag_dest_gtk.h"
#include "content/browser/tab_contents/web_drag_source_gtk.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDragData.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebInputEvent.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/login/user_manager.h"
#endif

using WebKit::WebDragOperation;
using WebKit::WebDragOperationsMask;
using WebKit::WebInputEvent;

namespace {

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

// See tab_contents_view_gtk.cc for discussion of mouse scroll zooming.
gboolean OnMouseScroll(GtkWidget* widget, GdkEventScroll* event,
                       internal::NativeTabContentsViewDelegate* delegate) {
  if ((event->state & gtk_accelerator_get_default_mod_mask()) ==
      GDK_CONTROL_MASK) {
    if (event->direction == GDK_SCROLL_DOWN) {
      delegate->OnNativeTabContentsViewWheelZoom(false);
      return TRUE;
    }
    if (event->direction == GDK_SCROLL_UP) {
      delegate->OnNativeTabContentsViewWheelZoom(true);
      return TRUE;
    }
  }

  return FALSE;
}

// Our dragging needs to use views datatypes instead of the default GTK ones.
class WebDragBookmarkHandlerViewGtk : public WebDragBookmarkHandlerGtk {
 public:
  virtual GdkAtom GetBookmarkTargetAtom() const {
    // For Views, bookmark drag data is encoded in the same format, and
    // associated with a custom format. See BookmarkNodeData::Write() for
    // details.
    return BookmarkNodeData::GetBookmarkCustomFormat();
  }
};

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// NativeTabContentsViewGtk, public:

NativeTabContentsViewGtk::NativeTabContentsViewGtk(
    internal::NativeTabContentsViewDelegate* delegate)
    : views::NativeWidgetGtk(delegate->AsNativeWidgetDelegate()),
      delegate_(delegate),
      ignore_next_char_event_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(drag_source_(
          new content::WebDragSourceGtk(delegate->GetTabContents()))) {
}

NativeTabContentsViewGtk::~NativeTabContentsViewGtk() {
  delegate_ = NULL;
}

void NativeTabContentsViewGtk::AttachConstrainedWindow(
    ConstrainedWindowGtk* constrained_window) {
  DCHECK(find(constrained_windows_.begin(), constrained_windows_.end(),
              constrained_window) == constrained_windows_.end());

  constrained_windows_.push_back(constrained_window);
  AddChild(constrained_window->widget());

  gfx::Size requested_size;
  views::NativeWidgetGtk::GetRequestedSize(&requested_size);
  PositionConstrainedWindows(requested_size);
}

void NativeTabContentsViewGtk::RemoveConstrainedWindow(
    ConstrainedWindowGtk* constrained_window) {
  std::vector<ConstrainedWindowGtk*>::iterator item =
      find(constrained_windows_.begin(), constrained_windows_.end(),
           constrained_window);
  DCHECK(item != constrained_windows_.end());
  RemoveChild((*item)->widget());
  constrained_windows_.erase(item);
}

////////////////////////////////////////////////////////////////////////////////
// NativeTabContentsViewGtk, NativeTabContentsView implementation:

void NativeTabContentsViewGtk::InitNativeTabContentsView() {
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_CONTROL);
  params.native_widget = this;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  GetWidget()->Init(params);

  // We need to own the widget in order to attach/detach the native view to a
  // container.
  gtk_object_ref(GTK_OBJECT(GetWidget()->GetNativeView()));
}

void NativeTabContentsViewGtk::Unparent() {
}

RenderWidgetHostView* NativeTabContentsViewGtk::CreateRenderWidgetHostView(
    RenderWidgetHost* render_widget_host) {
  RenderWidgetHostViewGtk* view =
      new RenderWidgetHostViewGtk(render_widget_host);
  view->InitAsChild();
  g_signal_connect(view->native_view(), "focus",
                   G_CALLBACK(OnFocus), delegate_->GetTabContents());
  g_signal_connect(view->native_view(), "scroll-event",
                   G_CALLBACK(OnMouseScroll), delegate_);

  // Let widget know that the tab contents has been painted.
  views::NativeWidgetGtk::RegisterChildExposeHandler(view->native_view());

  // Renderer target DnD.
  if (delegate_->GetTabContents()->ShouldAcceptDragAndDrop()) {
    drag_dest_.reset(new content::WebDragDestGtk(delegate_->GetTabContents(),
                                                 view->native_view()));
    bookmark_handler_gtk_.reset(new WebDragBookmarkHandlerGtk);
    drag_dest_->set_delegate(bookmark_handler_gtk_.get());
  }

  gtk_fixed_put(GTK_FIXED(GetWidget()->GetNativeView()), view->native_view(), 0,
                0);
  return view;
}

gfx::NativeWindow NativeTabContentsViewGtk::GetTopLevelNativeWindow() const {
  GtkWidget* window = gtk_widget_get_ancestor(GetWidget()->GetNativeView(),
                                              GTK_TYPE_WINDOW);
  return window ? GTK_WINDOW(window) : NULL;
}

void NativeTabContentsViewGtk::SetPageTitle(const string16& title) {
  // Set the window name to include the page title so it's easier to spot
  // when debugging (e.g. via xwininfo -tree).
  if (GDK_IS_WINDOW(GetNativeView()->window))
    gdk_window_set_title(GetNativeView()->window, UTF16ToUTF8(title).c_str());
}

void NativeTabContentsViewGtk::StartDragging(const WebDropData& drop_data,
                                             WebKit::WebDragOperationsMask ops,
                                             const SkBitmap& image,
                                             const gfx::Point& image_offset) {
  drag_source_->StartDragging(drop_data, ops, &last_mouse_down_,
                              image, image_offset);
}

void NativeTabContentsViewGtk::CancelDrag() {
}

bool NativeTabContentsViewGtk::IsDoingDrag() const {
  return false;
}

void NativeTabContentsViewGtk::SetDragCursor(
    WebKit::WebDragOperation operation) {
  if (drag_dest_.get())
    drag_dest_->UpdateDragStatus(operation);
}

views::NativeWidget* NativeTabContentsViewGtk::AsNativeWidget() {
  return this;
}

////////////////////////////////////////////////////////////////////////////////
// NativeTabContentsViewGtk, views::NativeWidgetGtk overrides:

// Called when the mouse moves within the widget. We notify SadTabView if it's
// not NULL, else our delegate.
gboolean NativeTabContentsViewGtk::OnMotionNotify(GtkWidget* widget,
                                                  GdkEventMotion* event) {
  if (delegate_->IsShowingSadTab())
    return views::NativeWidgetGtk::OnMotionNotify(widget, event);

  delegate_->OnNativeTabContentsViewMouseMove(true);
  return FALSE;
}

gboolean NativeTabContentsViewGtk::OnLeaveNotify(GtkWidget* widget,
                                                 GdkEventCrossing* event) {
  if (delegate_->IsShowingSadTab())
    return views::NativeWidgetGtk::OnLeaveNotify(widget, event);

  delegate_->OnNativeTabContentsViewMouseMove(false);
  return FALSE;
}

gboolean NativeTabContentsViewGtk::OnButtonPress(GtkWidget* widget,
                                                 GdkEventButton* event) {
  if (delegate_->IsShowingSadTab())
    return views::NativeWidgetGtk::OnButtonPress(widget, event);
  last_mouse_down_ = *event;
  return views::NativeWidgetGtk::OnButtonPress(widget, event);
}

void NativeTabContentsViewGtk::OnSizeAllocate(GtkWidget* widget,
                                              GtkAllocation* allocation) {
  gfx::Size size(allocation->width, allocation->height);
  delegate_->OnNativeTabContentsViewSized(size);
  if (size != size_)
    PositionConstrainedWindows(size);
  size_ = size;
  views::NativeWidgetGtk::OnSizeAllocate(widget, allocation);
}

void NativeTabContentsViewGtk::OnShow(GtkWidget* widget) {
  delegate_->OnNativeTabContentsViewShown();
  views::NativeWidgetGtk::OnShow(widget);
}

void NativeTabContentsViewGtk::OnHide(GtkWidget* widget) {
  // OnHide can be called during widget destruction (gtk_widget_dispose calls
  // gtk_widget_hide) so we make sure we do not call back through to the
  // delegate after it's already deleted.
  if (delegate_)
    delegate_->OnNativeTabContentsViewHidden();
  views::NativeWidgetGtk::OnHide(widget);
}

////////////////////////////////////////////////////////////////////////////////
// NativeTabContentsViewGtk, private:

void NativeTabContentsViewGtk::PositionConstrainedWindows(
    const gfx::Size& view_size) {
  // Place each ConstrainedWindow in the center of the view.
  int half_view_width = view_size.width() / 2;

  typedef std::vector<ConstrainedWindowGtk*>::iterator iterator;

  for (iterator f = constrained_windows_.begin(),
                l = constrained_windows_.end(); f != l; ++f) {
    GtkWidget* widget = (*f)->widget();

    GtkRequisition requisition;
    gtk_widget_size_request(widget, &requisition);

    int child_x = std::max(half_view_width - (requisition.width / 2), 0);
    int child_y = 0;

#if defined(OS_CHROMEOS)
    // TODO(ivankr): this is a hack to display proxy authentication dialog
    // centered during the sign-in phase. See http://crosbug/20819.
    if (!chromeos::UserManager::Get()->user_is_logged_in())
      child_y = std::max(view_size.height() / 2 - (requisition.height / 2), 0);
#endif
    PositionChild(widget, child_x, child_y, 0, 0);
  }
}

////////////////////////////////////////////////////////////////////////////////
// NativeTabContentsView, public:

// static
NativeTabContentsView* NativeTabContentsView::CreateNativeTabContentsView(
    internal::NativeTabContentsViewDelegate* delegate) {
  return new NativeTabContentsViewGtk(delegate);
}
