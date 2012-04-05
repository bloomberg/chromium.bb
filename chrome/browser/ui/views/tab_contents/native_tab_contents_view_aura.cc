// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tab_contents/native_tab_contents_view_aura.h"

#include "base/event_types.h"
#include "base/message_loop.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/ui/sad_tab_helper.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/browser/ui/views/tab_contents/native_tab_contents_view_delegate.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"
#include "content/public/browser/web_drag_dest_delegate.h"
#include "ui/aura/client/drag_drop_client.h"
#include "ui/aura/client/drag_drop_delegate.h"
#include "ui/aura/event.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/custom_data_helper.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/base/dragdrop/os_exchange_data_provider_aura.h"
#include "ui/gfx/compositor/layer.h"
#include "ui/views/views_delegate.h"
#include "ui/views/widget/widget.h"
#include "webkit/glue/webdropdata.h"

#if defined(USE_ASH)
#include "ash/shell.h"
#include "ash/wm/visibility_controller.h"
#endif

using content::RenderViewHost;
using content::RenderWidgetHostView;
using content::WebContents;

namespace {

// Listens to all mouse drag events during a drag and drop and sends them to
// the renderer.
class WebDragSourceAura : public MessageLoopForUI::Observer {
 public:
  explicit WebDragSourceAura(NativeTabContentsViewAura* view)
      : view_(view) {
    MessageLoopForUI::current()->AddObserver(this);
  }

  virtual ~WebDragSourceAura() {
    MessageLoopForUI::current()->RemoveObserver(this);
  }

  // MessageLoop::Observer implementation:
  virtual base::EventStatus WillProcessEvent(
      const base::NativeEvent& event) OVERRIDE {
    return base::EVENT_CONTINUE;
  }
  virtual void DidProcessEvent(const base::NativeEvent& event) OVERRIDE {
    ui::EventType type = ui::EventTypeFromNative(event);
    RenderViewHost* rvh = NULL;
    switch (type) {
      case ui::ET_MOUSE_DRAGGED:
        rvh = view_->GetWebContents()->GetRenderViewHost();
        if (rvh) {
          gfx::Point screen_loc = ui::EventLocationFromNative(event);
          gfx::Point client_loc = screen_loc;
          aura::Window* window = rvh->GetView()->GetNativeView();
          aura::Window::ConvertPointToWindow(window->GetRootWindow(),
              window, &client_loc);
          rvh->DragSourceMovedTo(client_loc.x(), client_loc.y(),
              screen_loc.x(), screen_loc.y());
        }
        break;
      default:
        break;
    }
  }


 private:
  NativeTabContentsViewAura* view_;

  DISALLOW_COPY_AND_ASSIGN(WebDragSourceAura);
};

// Utility to fill a ui::OSExchangeDataProviderAura object from WebDropData.
void PrepareDragData(const WebDropData& drop_data,
                     ui::OSExchangeDataProviderAura* provider) {
  if (!drop_data.plain_text.empty())
    provider->SetString(drop_data.plain_text);
  if (drop_data.url.is_valid())
    provider->SetURL(drop_data.url, drop_data.url_title);
  if (!drop_data.text_html.empty())
    provider->SetHtml(drop_data.text_html, drop_data.html_base_url);
  if (!drop_data.filenames.empty()) {
    std::vector<FilePath> paths;
    for (std::vector<string16>::const_iterator it = drop_data.filenames.begin();
        it != drop_data.filenames.end(); ++it)
      paths.push_back(FilePath::FromUTF8Unsafe(UTF16ToUTF8(*it)));
    provider->SetFilenames(paths);
  }
  if (!drop_data.custom_data.empty()) {
    Pickle pickle;
    ui::WriteCustomDataToPickle(drop_data.custom_data, &pickle);
    provider->SetPickledData(ui::Clipboard::GetWebCustomDataFormatType(),
                             pickle);
  }
}

// Utility to fill a WebDropData object from ui::OSExchangeData.
void PrepareWebDropData(WebDropData* drop_data,
                        const ui::OSExchangeData& data) {
  string16 plain_text, url_title;
  GURL url;

  data.GetString(&plain_text);
  if (!plain_text.empty())
    drop_data->plain_text = plain_text;

  data.GetURLAndTitle(&url, &url_title);
  if (url.is_valid()) {
    drop_data->url = url;
    drop_data->url_title = url_title;
  }

  data.GetHtml(&drop_data->text_html, &drop_data->html_base_url);

  std::vector<FilePath> files;
  if (data.GetFilenames(&files) && !files.empty()) {
    for (std::vector<FilePath>::const_iterator it = files.begin();
        it != files.end(); ++it)
      drop_data->filenames.push_back(UTF8ToUTF16(it->AsUTF8Unsafe()));
  }

  Pickle pickle;
  if (data.GetPickledData(ui::Clipboard::GetWebCustomDataFormatType(),
                          &pickle))
    ui::ReadCustomDataIntoMap(pickle.data(), pickle.size(),
                              &drop_data->custom_data);
}

// Utilities to convert between WebKit::WebDragOperationsMask and
// ui::DragDropTypes.
int ConvertFromWeb(WebKit::WebDragOperationsMask ops) {
  int drag_op = ui::DragDropTypes::DRAG_NONE;
  if (ops & WebKit::WebDragOperationCopy)
    drag_op |= ui::DragDropTypes::DRAG_COPY;
  if (ops & WebKit::WebDragOperationMove)
    drag_op |= ui::DragDropTypes::DRAG_MOVE;
  if (ops & WebKit::WebDragOperationLink)
    drag_op |= ui::DragDropTypes::DRAG_LINK;
  return drag_op;
}

WebKit::WebDragOperationsMask ConvertToWeb(int drag_op) {
  int web_drag_op = WebKit::WebDragOperationNone;
  if (drag_op & ui::DragDropTypes::DRAG_COPY)
    web_drag_op |= WebKit::WebDragOperationCopy;
  if (drag_op & ui::DragDropTypes::DRAG_MOVE)
    web_drag_op |= WebKit::WebDragOperationMove;
  if (drag_op & ui::DragDropTypes::DRAG_LINK)
    web_drag_op |= WebKit::WebDragOperationLink;
  return (WebKit::WebDragOperationsMask) web_drag_op;
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// NativeTabContentsViewAura, public:

NativeTabContentsViewAura::NativeTabContentsViewAura(
    internal::NativeTabContentsViewDelegate* delegate)
    : views::NativeWidgetAura(delegate->AsNativeWidgetDelegate()),
      delegate_(delegate),
      current_drag_op_(WebKit::WebDragOperationNone),
      drag_dest_delegate_(NULL),
      should_do_drag_cleanup_(NULL) {
}

NativeTabContentsViewAura::~NativeTabContentsViewAura() {
  if (should_do_drag_cleanup_)
    *should_do_drag_cleanup_ = false;
}

WebContents* NativeTabContentsViewAura::GetWebContents() const {
  return delegate_->GetWebContents();
}

////////////////////////////////////////////////////////////////////////////////
// NativeTabContentsViewAura, NativeTabContentsView implementation:

void NativeTabContentsViewAura::InitNativeTabContentsView() {
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_CONTROL);
  params.native_widget = this;
  // We don't draw anything so we don't need a texture.
  params.layer_type = ui::LAYER_NOT_DRAWN;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.parent = NULL;
  params.can_activate = true;
  GetWidget()->Init(params);
  GetNativeView()->layer()->SetMasksToBounds(true);
  GetNativeWindow()->SetName("NativeTabContentsViewAura");
#if defined(USE_ASH)
  ash::SetChildWindowVisibilityChangesAnimated(GetWidget()->GetNativeView());
#else
  NOTIMPLEMENTED() << "Need to animate in";
#endif

  if (delegate_)
    drag_dest_delegate_ = delegate_->GetDragDestDelegate();

  // Hide the widget to prevent it from showing up on the root window. This is
  // needed for TabContentses that aren't immediately added to the tabstrip,
  // e.g. the Instant preview contents.
  // TODO(beng): investigate if control-type windows shouldn't be hidden by
  //             default if they are created with no parent. Pending oshima's
  //             change to reflect Widget types onto a ViewProp.
  GetWidget()->Hide();
}

RenderWidgetHostView* NativeTabContentsViewAura::CreateRenderWidgetHostView(
    content::RenderWidgetHost* render_widget_host) {
  RenderWidgetHostView* view =
      RenderWidgetHostView::CreateViewForWidget(render_widget_host);

  view->InitAsChild(NULL);
  GetNativeView()->AddChild(view->GetNativeView());
  view->Show();

  // We listen to drag drop events in the newly created view's window.
  aura::client::SetDragDropDelegate(view->GetNativeView(), this);
  return view;
}

gfx::NativeWindow NativeTabContentsViewAura::GetTopLevelNativeWindow() const {
  // TODO(beng):
  NOTIMPLEMENTED();
  return NULL;
}

void NativeTabContentsViewAura::SetPageTitle(const string16& title) {
  GetNativeView()->set_title(title);
}

void NativeTabContentsViewAura::StartDragging(const WebDropData& drop_data,
                                             WebKit::WebDragOperationsMask ops,
                                             const SkBitmap& image,
                                             const gfx::Point& image_offset) {
  aura::RootWindow* root_window = GetNativeView()->GetRootWindow();
  if (!aura::client::GetDragDropClient(root_window))
    return;

  ui::OSExchangeDataProviderAura* provider = new ui::OSExchangeDataProviderAura;
  PrepareDragData(drop_data, provider);
  if (!image.isNull())
    provider->set_drag_image(image);
  ui::OSExchangeData data(provider);  // takes ownership of |provider|.

  scoped_ptr<WebDragSourceAura> drag_source(new WebDragSourceAura(this));

  bool should_do_drag_cleanup = true;
  should_do_drag_cleanup_ = &should_do_drag_cleanup;

  // We need to enable recursive tasks on the message loop so we can get
  // updates while in the system DoDragDrop loop.
  int result_op = 0;
  {
    // TODO(sad): Avoid using last_mouse_location here, since the drag may not
    // always start from a mouse-event (e.g. a touch or gesture event could
    // initiate the drag). The location information should be carried over from
    // webkit. http://crbug.com/114754
    gfx::Point location(root_window->last_mouse_location());
    location.Offset(-image_offset.x(), -image_offset.y());
    MessageLoop::ScopedNestableTaskAllower allow(MessageLoop::current());
    result_op = aura::client::GetDragDropClient(root_window)->StartDragAndDrop(
        data, location, ConvertFromWeb(ops));
  }

  if (should_do_drag_cleanup)
    EndDrag(ConvertToWeb(result_op));
  else
    should_do_drag_cleanup_ = NULL;
}

void NativeTabContentsViewAura::CancelDrag() {
  aura::RootWindow* root_window = GetNativeView()->GetRootWindow();
  if (aura::client::GetDragDropClient(root_window))
    aura::client::GetDragDropClient(root_window)->DragCancel();
}

bool NativeTabContentsViewAura::IsDoingDrag() const {
  aura::RootWindow* root_window = GetNativeView()->GetRootWindow();
  if (aura::client::GetDragDropClient(root_window))
    return aura::client::GetDragDropClient(root_window)->IsDragDropInProgress();
  return false;
}

void NativeTabContentsViewAura::SetDragCursor(
    WebKit::WebDragOperation operation) {
  current_drag_op_ = operation;
}

views::NativeWidget* NativeTabContentsViewAura::AsNativeWidget() {
  return this;
}

////////////////////////////////////////////////////////////////////////////////
// NativeTabContentsViewAura, views::NativeWidgetAura overrides:

void NativeTabContentsViewAura::OnBoundsChanged(const gfx::Rect& old_bounds,
                                                const gfx::Rect& new_bounds) {
  delegate_->OnNativeTabContentsViewSized(new_bounds.size());
  views::NativeWidgetAura::OnBoundsChanged(old_bounds, new_bounds);
}

bool NativeTabContentsViewAura::OnMouseEvent(aura::MouseEvent* event) {
  TabContentsWrapper* wrapper =
      TabContentsWrapper::GetCurrentWrapperForContents(GetWebContents());
  if (!wrapper || !wrapper->sad_tab_helper()->HasSadTab()) {
    switch (event->type()) {
      case ui::ET_MOUSE_EXITED:
        delegate_->OnNativeTabContentsViewMouseMove(false);
        break;
      case ui::ET_MOUSE_MOVED:
        delegate_->OnNativeTabContentsViewMouseMove(true);
        break;
      default:
        // TODO(oshima): mouse wheel
        break;
    }
  }
  // Pass all mouse event to renderer.
  return views::NativeWidgetAura::OnMouseEvent(event);
}

void NativeTabContentsViewAura::OnDragEntered(
    const aura::DropTargetEvent& event) {
  if (drag_dest_delegate_)
    drag_dest_delegate_->DragInitialize(GetWebContents());

  WebDropData drop_data;
  PrepareWebDropData(&drop_data, event.data());
  WebKit::WebDragOperationsMask op = ConvertToWeb(event.source_operations());

  gfx::Point screen_pt =
      GetNativeView()->GetRootWindow()->last_mouse_location();
  GetWebContents()->GetRenderViewHost()->DragTargetDragEnter(
      drop_data, event.location(), screen_pt, op);

  if (drag_dest_delegate_) {
    drag_dest_delegate_->OnReceiveDragData(event.data());
    drag_dest_delegate_->OnDragEnter();
  }
}

int NativeTabContentsViewAura::OnDragUpdated(
    const aura::DropTargetEvent& event) {
  WebKit::WebDragOperationsMask op = ConvertToWeb(event.source_operations());
  gfx::Point screen_pt =
      GetNativeView()->GetRootWindow()->last_mouse_location();
  GetWebContents()->GetRenderViewHost()->DragTargetDragOver(
      event.location(), screen_pt, op);

  if (drag_dest_delegate_)
    drag_dest_delegate_->OnDragOver();

  return ConvertFromWeb(current_drag_op_);
}

void NativeTabContentsViewAura::OnDragExited() {
  GetWebContents()->GetRenderViewHost()->DragTargetDragLeave();
  if (drag_dest_delegate_)
    drag_dest_delegate_->OnDragLeave();
}

int NativeTabContentsViewAura::OnPerformDrop(
    const aura::DropTargetEvent& event) {
  GetWebContents()->GetRenderViewHost()->DragTargetDrop(
      event.location(),
      GetNativeView()->GetRootWindow()->last_mouse_location());
  if (drag_dest_delegate_)
    drag_dest_delegate_->OnDrop();
  return current_drag_op_;
}

void NativeTabContentsViewAura::OnWindowDestroying() {
  if (should_do_drag_cleanup_)
    *should_do_drag_cleanup_ = false;
  NativeWidgetAura::OnWindowDestroying();
}

////////////////////////////////////////////////////////////////////////////////
// NativeTabContentsViewAura, private:

void NativeTabContentsViewAura::EndDrag(WebKit::WebDragOperationsMask ops) {
  if (!GetWebContents() || !GetWebContents()->GetRenderViewHost())
    return;
  aura::RootWindow* root_window = GetNativeView()->GetRootWindow();
  if (!root_window)
    return;
  gfx::Point screen_loc = root_window->last_mouse_location();
  gfx::Point client_loc = screen_loc;
  RenderViewHost* rvh = GetWebContents()->GetRenderViewHost();
  aura::Window* window = rvh->GetView()->GetNativeView();
  aura::Window::ConvertPointToWindow(root_window, window, &client_loc);
  rvh->DragSourceEndedAt(client_loc.x(), client_loc.y(), screen_loc.x(),
      screen_loc.y(), ops);
  rvh->DragSourceSystemDragEnded();
}

////////////////////////////////////////////////////////////////////////////////
// NativeTabContentsView, public:

// static
NativeTabContentsView* NativeTabContentsView::CreateNativeTabContentsView(
    internal::NativeTabContentsViewDelegate* delegate) {
  return new NativeTabContentsViewAura(delegate);
}
