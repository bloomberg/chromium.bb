// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/tab_contents/tab_contents_view_aura.h"

#include "base/utf_string_conversions.h"
#include "content/browser/renderer_host/render_view_host_factory.h"
#include "content/browser/tab_contents/interstitial_page_impl.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_view_delegate.h"
#include "content/public/browser/web_drag_dest_delegate.h"
#include "ui/aura/client/drag_drop_client.h"
#include "ui/aura/client/drag_drop_delegate.h"
#include "ui/aura/event.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#include "ui/base/clipboard/custom_data_helper.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/base/dragdrop/os_exchange_data_provider_aura.h"
#include "ui/base/hit_test.h"
#include "ui/gfx/compositor/layer.h"
#include "ui/gfx/screen.h"
#include "webkit/glue/webdropdata.h"

namespace {

// Listens to all mouse drag events during a drag and drop and sends them to
// the renderer.
class WebDragSourceAura : public MessageLoopForUI::Observer {
 public:
  explicit WebDragSourceAura(TabContents* contents)
      : contents_(contents) {
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
    content::RenderViewHost* rvh = NULL;
    switch (type) {
      case ui::ET_MOUSE_DRAGGED:
        rvh = contents_->GetRenderViewHost();
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
  TabContents* contents_;

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
// TabContentsViewAura, public:

TabContentsViewAura::TabContentsViewAura(
    TabContents* tab_contents,
    content::WebContentsViewDelegate* delegate)
    : tab_contents_(tab_contents),
      view_(NULL),
      delegate_(delegate),
      current_drag_op_(WebKit::WebDragOperationNone),
      close_tab_after_drag_ends_(false) {
}

TabContentsViewAura::~TabContentsViewAura() {
}

////////////////////////////////////////////////////////////////////////////////
// TabContentsViewAura, private:

void TabContentsViewAura::SizeChangedCommon(const gfx::Size& size) {
  if (tab_contents_->GetInterstitialPage())
    tab_contents_->GetInterstitialPage()->SetSize(size);
  content::RenderWidgetHostView* rwhv =
      tab_contents_->GetRenderWidgetHostView();
  if (rwhv)
    rwhv->SetSize(size);
}

void TabContentsViewAura::EndDrag(WebKit::WebDragOperationsMask ops) {
  aura::RootWindow* root_window = GetNativeView()->GetRootWindow();
  gfx::Point screen_loc = root_window->last_mouse_location();
  gfx::Point client_loc = screen_loc;
  content::RenderViewHost* rvh = tab_contents_->GetRenderViewHost();
  aura::Window* window = rvh->GetView()->GetNativeView();
  aura::Window::ConvertPointToWindow(root_window, window, &client_loc);
  rvh->DragSourceEndedAt(client_loc.x(), client_loc.y(), screen_loc.x(),
      screen_loc.y(), ops);
}

content::WebDragDestDelegate* TabContentsViewAura::GetDragDestDelegate() {
  return delegate_.get() ? delegate_->GetDragDestDelegate() : NULL;
}

////////////////////////////////////////////////////////////////////////////////
// TabContentsViewAura, WebContentsView implementation:

void TabContentsViewAura::CreateView(const gfx::Size& initial_size) {
  initial_size_ = initial_size;

  window_.reset(new aura::Window(this));
  window_->SetType(aura::client::WINDOW_TYPE_CONTROL);
  window_->SetTransparent(false);
  window_->Init(ui::LAYER_TEXTURED);
  window_->layer()->SetMasksToBounds(true);
  window_->SetName("TabContentsViewAura");

  // TODO(beng): allow for delegate init step.

  // TODO(beng): drag & drop init.
}

content::RenderWidgetHostView* TabContentsViewAura::CreateViewForWidget(
    content::RenderWidgetHost* render_widget_host) {
  if (render_widget_host->GetView()) {
    // During testing, the view will already be set up in most cases to the
    // test view, so we don't want to clobber it with a real one. To verify that
    // this actually is happening (and somebody isn't accidentally creating the
    // view twice), we check for the RVH Factory, which will be set when we're
    // making special ones (which go along with the special views).
    DCHECK(RenderViewHostFactory::has_factory());
    return render_widget_host->GetView();
  }

  view_ = content::RenderWidgetHostView::CreateViewForWidget(
      render_widget_host);
  view_->InitAsChild(NULL);
  GetNativeView()->AddChild(view_->GetNativeView());
  view_->Show();

  // We listen to drag drop events in the newly created view's window.
  aura::client::SetDragDropDelegate(view_->GetNativeView(), this);
  return view_;
}

gfx::NativeView TabContentsViewAura::GetNativeView() const {
  return window_.get();
}

gfx::NativeView TabContentsViewAura::GetContentNativeView() const {
  return view_->GetNativeView();
}

gfx::NativeWindow TabContentsViewAura::GetTopLevelNativeWindow() const {
  return window_->GetToplevelWindow();
}

void TabContentsViewAura::GetContainerBounds(gfx::Rect *out) const {
  *out = window_->GetScreenBounds();
}

void TabContentsViewAura::SetPageTitle(const string16& title) {
  window_->set_title(title);
}

void TabContentsViewAura::OnTabCrashed(base::TerminationStatus status,
                                       int error_code) {
  view_ = NULL;
}

void TabContentsViewAura::SizeContents(const gfx::Size& size) {
  gfx::Rect bounds = window_->bounds();
  if (bounds.size() != size) {
    bounds.set_size(size);
    window_->SetBounds(bounds);
  } else {
    // Our size matches what we want but the renderers size may not match.
    // Pretend we were resized so that the renderers size is updated too.
    SizeChangedCommon(size);

  }
}

void TabContentsViewAura::RenderViewCreated(content::RenderViewHost* host) {
}

void TabContentsViewAura::Focus() {
  if (tab_contents_->GetInterstitialPage()) {
    tab_contents_->GetInterstitialPage()->Focus();
    return;
  }

  if (delegate_.get() && delegate_->Focus())
    return;

  content::RenderWidgetHostView* rwhv =
      tab_contents_->GetRenderWidgetHostView();
  if (rwhv)
    rwhv->Focus();
}

void TabContentsViewAura::SetInitialFocus() {
  if (tab_contents_->FocusLocationBarByDefault())
    tab_contents_->SetFocusToLocationBar(false);
  else
    Focus();
}

void TabContentsViewAura::StoreFocus() {
  if (delegate_.get())
    delegate_->StoreFocus();
}

void TabContentsViewAura::RestoreFocus() {
  if (delegate_.get())
    delegate_->RestoreFocus();
}

bool TabContentsViewAura::IsDoingDrag() const {
  aura::RootWindow* root_window = GetNativeView()->GetRootWindow();
  if (aura::client::GetDragDropClient(root_window))
    return aura::client::GetDragDropClient(root_window)->IsDragDropInProgress();
  return false;
}

void TabContentsViewAura::CancelDragAndCloseTab() {
  DCHECK(IsDoingDrag());
  // We can't close the tab while we're in the drag and
  // |drag_handler_->CancelDrag()| is async.  Instead, set a flag to cancel
  // the drag and when the drag nested message loop ends, close the tab.
  aura::RootWindow* root_window = GetNativeView()->GetRootWindow();
  if (aura::client::GetDragDropClient(root_window))
    aura::client::GetDragDropClient(root_window)->DragCancel();

  close_tab_after_drag_ends_ = true;
}

bool TabContentsViewAura::IsEventTracking() const {
  return false;
}

void TabContentsViewAura::CloseTabAfterEventTracking() {
}

void TabContentsViewAura::GetViewBounds(gfx::Rect* out) const {
  *out = window_->GetScreenBounds();
}

////////////////////////////////////////////////////////////////////////////////
// TabContentsViewAura, RenderViewHostDelegate::View implementation:

void TabContentsViewAura::CreateNewWindow(
    int route_id,
    const ViewHostMsg_CreateWindow_Params& params) {
  tab_contents_view_helper_.CreateNewWindow(tab_contents_, route_id, params);
}

void TabContentsViewAura::CreateNewWidget(int route_id,
                                          WebKit::WebPopupType popup_type) {
  tab_contents_view_helper_.CreateNewWidget(tab_contents_,
                                            route_id,
                                            false,
                                            popup_type);
}

void TabContentsViewAura::CreateNewFullscreenWidget(int route_id) {
  tab_contents_view_helper_.CreateNewWidget(tab_contents_,
                                            route_id,
                                            true,
                                            WebKit::WebPopupTypeNone);
}

void TabContentsViewAura::ShowCreatedWindow(int route_id,
                                            WindowOpenDisposition disposition,
                                            const gfx::Rect& initial_pos,
                                            bool user_gesture) {
  tab_contents_view_helper_.ShowCreatedWindow(
      tab_contents_, route_id, disposition, initial_pos, user_gesture);
}

void TabContentsViewAura::ShowCreatedWidget(int route_id,
                                            const gfx::Rect& initial_pos) {
  tab_contents_view_helper_.ShowCreatedWidget(tab_contents_,
                                              route_id,
                                              false,
                                              initial_pos);
}

void TabContentsViewAura::ShowCreatedFullscreenWidget(int route_id) {
  tab_contents_view_helper_.ShowCreatedWidget(tab_contents_,
                                              route_id,
                                              true,
                                              gfx::Rect());
}

void TabContentsViewAura::ShowContextMenu(
    const content::ContextMenuParams& params) {
  // Allow WebContentsDelegates to handle the context menu operation first.
  if (tab_contents_->GetDelegate() &&
      tab_contents_->GetDelegate()->HandleContextMenu(params)) {
    return;
  }

  if (delegate_.get())
    delegate_->ShowContextMenu(params);
}

void TabContentsViewAura::ShowPopupMenu(const gfx::Rect& bounds,
                                        int item_height,
                                        double item_font_size,
                                        int selected_item,
                                        const std::vector<WebMenuItem>& items,
                                        bool right_aligned) {
  // External popup menus are only used on Mac.
  NOTIMPLEMENTED();
}

void TabContentsViewAura::StartDragging(
    const WebDropData& drop_data,
    WebKit::WebDragOperationsMask operations,
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

  scoped_ptr<WebDragSourceAura> drag_source(
      new WebDragSourceAura(tab_contents_));

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
        data, location, ConvertFromWeb(operations));
  }

  EndDrag(ConvertToWeb(result_op));
  tab_contents_->GetRenderViewHost()->DragSourceSystemDragEnded();}

void TabContentsViewAura::UpdateDragCursor(WebKit::WebDragOperation operation) {
  current_drag_op_ = operation;
}

void TabContentsViewAura::GotFocus() {
  if (tab_contents_->GetDelegate())
    tab_contents_->GetDelegate()->WebContentsFocused(tab_contents_);
}

void TabContentsViewAura::TakeFocus(bool reverse) {
  if (tab_contents_->GetDelegate() &&
      !tab_contents_->GetDelegate()->TakeFocus(reverse) &&
      delegate_.get()) {
    delegate_->TakeFocus(reverse);
  }
}

////////////////////////////////////////////////////////////////////////////////
// TabContentsViewAura, aura::WindowDelegate implementation:

gfx::Size TabContentsViewAura::GetMinimumSize() const {
  return gfx::Size();
}

void TabContentsViewAura::OnBoundsChanged(const gfx::Rect& old_bounds,
                                          const gfx::Rect& new_bounds) {
  SizeChangedCommon(new_bounds.size());
  if (delegate_.get())
    delegate_->SizeChanged(new_bounds.size());
}

void TabContentsViewAura::OnFocus() {
}

void TabContentsViewAura::OnBlur() {
}

bool TabContentsViewAura::OnKeyEvent(aura::KeyEvent* event) {
  return false;
}

gfx::NativeCursor TabContentsViewAura::GetCursor(const gfx::Point& point) {
  return NULL;
}

int TabContentsViewAura::GetNonClientComponent(const gfx::Point& point) const {
  return HTCLIENT;
}

bool TabContentsViewAura::OnMouseEvent(aura::MouseEvent* event) {
  if (!tab_contents_->GetDelegate())
    return false;

  switch (event->type()) {
    case ui::ET_MOUSE_PRESSED:
      tab_contents_->GetDelegate()->ActivateContents(tab_contents_);
      break;
    case ui::ET_MOUSE_MOVED:
      tab_contents_->GetDelegate()->ContentsMouseEvent(
          tab_contents_, gfx::Screen::GetCursorScreenPoint(), true);
      break;
    default:
      break;
  }
  return false;
}

ui::TouchStatus TabContentsViewAura::OnTouchEvent(aura::TouchEvent* event) {
  return ui::TOUCH_STATUS_UNKNOWN;
}

ui::GestureStatus TabContentsViewAura::OnGestureEvent(
    aura::GestureEvent* event) {
  return ui::GESTURE_STATUS_UNKNOWN;
}

bool TabContentsViewAura::CanFocus() {
  return false;
}

void TabContentsViewAura::OnCaptureLost() {
}

void TabContentsViewAura::OnPaint(gfx::Canvas* canvas) {
}

void TabContentsViewAura::OnWindowDestroying() {
}

void TabContentsViewAura::OnWindowDestroyed() {
}

void TabContentsViewAura::OnWindowVisibilityChanged(bool visible) {
  if (visible)
    tab_contents_->ShowContents();
  else
    tab_contents_->HideContents();
}
////////////////////////////////////////////////////////////////////////////////
// TabContentsViewAura, aura::client::DragDropDelegate implementation:

void TabContentsViewAura::OnDragEntered(const aura::DropTargetEvent& event) {
  if (GetDragDestDelegate())
    GetDragDestDelegate()->DragInitialize(tab_contents_);

  WebDropData drop_data;
  PrepareWebDropData(&drop_data, event.data());
  WebKit::WebDragOperationsMask op = ConvertToWeb(event.source_operations());

  gfx::Point screen_pt =
      GetNativeView()->GetRootWindow()->last_mouse_location();
  tab_contents_->GetRenderViewHost()->DragTargetDragEnter(
      drop_data, event.location(), screen_pt, op);

  if (GetDragDestDelegate()) {
    GetDragDestDelegate()->OnReceiveDragData(event.data());
    GetDragDestDelegate()->OnDragEnter();
  }
}

int TabContentsViewAura::OnDragUpdated(const aura::DropTargetEvent& event) {
  WebKit::WebDragOperationsMask op = ConvertToWeb(event.source_operations());
  gfx::Point screen_pt =
      GetNativeView()->GetRootWindow()->last_mouse_location();
  tab_contents_->GetRenderViewHost()->DragTargetDragOver(
      event.location(), screen_pt, op);

  if (GetDragDestDelegate())
    GetDragDestDelegate()->OnDragOver();

  return ConvertFromWeb(current_drag_op_);
}

void TabContentsViewAura::OnDragExited() {
  tab_contents_->GetRenderViewHost()->DragTargetDragLeave();
  if (GetDragDestDelegate())
    GetDragDestDelegate()->OnDragLeave();
}

int TabContentsViewAura::OnPerformDrop(const aura::DropTargetEvent& event) {
  tab_contents_->GetRenderViewHost()->DragTargetDrop(
      event.location(),
      GetNativeView()->GetRootWindow()->last_mouse_location());
  if (GetDragDestDelegate())
    GetDragDestDelegate()->OnDrop();
  return current_drag_op_;
}
