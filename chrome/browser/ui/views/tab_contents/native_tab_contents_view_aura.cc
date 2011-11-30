// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tab_contents/native_tab_contents_view_aura.h"

#include "base/event_types.h"
#include "base/message_loop.h"
#include "chrome/browser/ui/views/tab_contents/native_tab_contents_view_delegate.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/renderer_host/render_widget_host_view_aura.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/browser/tab_contents/tab_contents_view.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/drag_drop_client.h"
#include "ui/aura/desktop.h"
#include "ui/aura/event.h"
#include "ui/aura/window.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/base/dragdrop/os_exchange_data_provider_aura.h"
#include "ui/views/widget/widget.h"
#include "views/views_delegate.h"
#include "webkit/glue/webdropdata.h"

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
        rvh = view_->GetTabContents()->render_view_host();
        if (rvh) {
          gfx::Point screen_loc = ui::EventLocationFromNative(event);
          gfx::Point client_loc = screen_loc;
          aura::Window* window = rvh->view()->GetNativeView();
          aura::Window::ConvertPointToWindow(aura::Desktop::GetInstance(),
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
  // TODO(varunjain): support other formats.
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
  // TODO(varunjain): support other formats.
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
      current_drag_op_(WebKit::WebDragOperationNone) {
}

NativeTabContentsViewAura::~NativeTabContentsViewAura() {
}

TabContents* NativeTabContentsViewAura::GetTabContents() const {
  return delegate_->GetTabContents();
}

////////////////////////////////////////////////////////////////////////////////
// NativeTabContentsViewAura, NativeTabContentsView implementation:

void NativeTabContentsViewAura::InitNativeTabContentsView() {
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_CONTROL);
  params.native_widget = this;
  // We don't draw anything so we don't need a texture.
  params.create_texture_for_layer = false;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.parent = NULL;
  GetWidget()->Init(params);

  // Hide the widget to prevent it from showing up on the desktop. This is
  // needed for TabContentses that aren't immediately added to the tabstrip,
  // e.g. the Instant preview contents.
  // TODO(beng): investigate if control-type windows shouldn't be hidden by
  //             default if they are created with no parent. Pending oshima's
  //             change to reflect Widget types onto a ViewProp.
  GetWidget()->Hide();
}

void NativeTabContentsViewAura::Unparent() {
  // Nothing to do.
}

RenderWidgetHostView* NativeTabContentsViewAura::CreateRenderWidgetHostView(
    RenderWidgetHost* render_widget_host) {
  RenderWidgetHostViewAura* view =
      new RenderWidgetHostViewAura(render_widget_host);
  view->InitAsChild();
  GetNativeView()->AddChild(view->GetNativeView());
  view->Show();

  // We listen to drag drop events in the newly created view's window.
  aura::Window* window = static_cast<aura::Window*>(view->GetNativeView());
  DCHECK(window);
  window->SetProperty(aura::kDragDropDelegateKey,
      static_cast<aura::WindowDragDropDelegate*>(this));
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
  aura::DragDropClient* client = static_cast<aura::DragDropClient*>(
      aura::Desktop::GetInstance()->GetProperty(
          aura::kDesktopDragDropClientKey));
  if (!client)
    return;

  ui::OSExchangeDataProviderAura* provider = new ui::OSExchangeDataProviderAura;
  PrepareDragData(drop_data, provider);
  if (!image.isNull())
    provider->set_drag_image(image);
  ui::OSExchangeData data(provider);  // takes ownership of |provider|.

  scoped_ptr<WebDragSourceAura> drag_source(new WebDragSourceAura(this));

  // We need to enable recursive tasks on the message loop so we can get
  // updates while in the system DoDragDrop loop.
  bool old_state = MessageLoop::current()->NestableTasksAllowed();
  MessageLoop::current()->SetNestableTasksAllowed(true);
  int result_op = client->StartDragAndDrop(data, ConvertFromWeb(ops));
  MessageLoop::current()->SetNestableTasksAllowed(old_state);

  EndDrag(ConvertToWeb(result_op));
  GetTabContents()->render_view_host()->DragSourceSystemDragEnded();
}

void NativeTabContentsViewAura::CancelDrag() {
  aura::DragDropClient* client = static_cast<aura::DragDropClient*>(
      aura::Desktop::GetInstance()->GetProperty(
          aura::kDesktopDragDropClientKey));
  if (client)
    client->DragCancel();
}

bool NativeTabContentsViewAura::IsDoingDrag() const {
  aura::DragDropClient* client = static_cast<aura::DragDropClient*>(
      aura::Desktop::GetInstance()->GetProperty(
          aura::kDesktopDragDropClientKey));
  if (client)
    return client->IsDragDropInProgress();
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
  if (!delegate_->IsShowingSadTab()) {
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
  WebDropData drop_data;
  PrepareWebDropData(&drop_data, event.data());
  WebKit::WebDragOperationsMask op = ConvertToWeb(event.source_operations());

  gfx::Point screen_pt = aura::Desktop::GetInstance()->last_mouse_location();
  GetTabContents()->render_view_host()->DragTargetDragEnter(
      drop_data, event.location(), screen_pt, op);
}

int NativeTabContentsViewAura::OnDragUpdated(
    const aura::DropTargetEvent& event) {
  WebKit::WebDragOperationsMask op = ConvertToWeb(event.source_operations());
  gfx::Point screen_pt = aura::Desktop::GetInstance()->last_mouse_location();
  GetTabContents()->render_view_host()->DragTargetDragOver(
      event.location(), screen_pt, op);
  return ConvertFromWeb(current_drag_op_);
}

void NativeTabContentsViewAura::OnDragExited() {
  GetTabContents()->render_view_host()->DragTargetDragLeave();
}

int NativeTabContentsViewAura::OnPerformDrop(
    const aura::DropTargetEvent& event) {
  GetTabContents()->render_view_host()->DragTargetDrop(
      event.location(), aura::Desktop::GetInstance()->last_mouse_location());
  return current_drag_op_;
}

////////////////////////////////////////////////////////////////////////////////
// NativeTabContentsViewAura, private:

void NativeTabContentsViewAura::EndDrag(WebKit::WebDragOperationsMask ops) {
  gfx::Point screen_loc = aura::Desktop::GetInstance()->last_mouse_location();
  gfx::Point client_loc = screen_loc;
  RenderViewHost* rvh = GetTabContents()->render_view_host();
  aura::Window* window = rvh->view()->GetNativeView();
  aura::Window::ConvertPointToWindow(aura::Desktop::GetInstance(),
      window, &client_loc);
  rvh->DragSourceEndedAt(client_loc.x(), client_loc.y(), screen_loc.x(),
      screen_loc.y(), ops);
}

////////////////////////////////////////////////////////////////////////////////
// NativeTabContentsView, public:

// static
NativeTabContentsView* NativeTabContentsView::CreateNativeTabContentsView(
    internal::NativeTabContentsViewDelegate* delegate) {
  return new NativeTabContentsViewAura(delegate);
}
