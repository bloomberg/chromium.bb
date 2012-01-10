// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_widget_host_view_aura.h"

#include "base/logging.h"
#include "content/browser/renderer_host/backing_store_skia.h"
#include "content/browser/renderer_host/render_widget_host.h"
#include "content/browser/renderer_host/web_input_event_aura.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "content/common/gpu/gpu_messages.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebCompositionUnderline.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebInputEvent.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebScreenInfo.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/tooltip_client.h"
#include "ui/aura/client/window_types.h"
#include "ui/aura/event.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#include "ui/base/hit_test.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/compositor/layer.h"
#include "ui/gfx/screen.h"

#if defined(UI_COMPOSITOR_IMAGE_TRANSPORT)
#include "base/bind.h"
#include "content/browser/renderer_host/accelerated_surface_container_linux.h"
#include "ui/gfx/gl/gl_bindings.h"
#endif

using WebKit::WebTouchEvent;

namespace {

ui::TouchStatus DecideTouchStatus(const WebKit::WebTouchEvent& event,
                                  WebKit::WebTouchPoint* point) {
  if (event.type == WebKit::WebInputEvent::TouchStart &&
      event.touchesLength == 1)
    return ui::TOUCH_STATUS_START;

  if (event.type == WebKit::WebInputEvent::TouchMove && point == NULL)
    return ui::TOUCH_STATUS_CONTINUE;

  if (event.type == WebKit::WebInputEvent::TouchEnd &&
      event.touchesLength == 0)
    return ui::TOUCH_STATUS_END;

  if (event.type == WebKit::WebInputEvent::TouchCancel)
    return ui::TOUCH_STATUS_CANCEL;

  return point ? ui::TOUCH_STATUS_CONTINUE : ui::TOUCH_STATUS_UNKNOWN;
}

void UpdateWebTouchEventAfterDispatch(WebKit::WebTouchEvent* event,
                                      WebKit::WebTouchPoint* point) {
  if (point->state != WebKit::WebTouchPoint::StateReleased)
    return;
  --event->touchesLength;
  for (unsigned i = point - event->touches;
       i < event->touchesLength;
       ++i) {
    event->touches[i] = event->touches[i + 1];
  }
}

bool CanRendererHandleEvent(const base::NativeEvent& native_event) {
#if defined(OS_WIN)
  // Renderer cannot handle WM_XBUTTON events.
  switch (native_event.message) {
    case WM_XBUTTONDOWN:
    case WM_XBUTTONUP:
    case WM_XBUTTONDBLCLK:
    case WM_NCXBUTTONDOWN:
    case WM_NCXBUTTONUP:
    case WM_NCXBUTTONDBLCLK:
      return false;
    default:
      break;
  }
#endif
  return true;
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// RenderWidgetHostViewAura, public:

RenderWidgetHostViewAura::RenderWidgetHostViewAura(RenderWidgetHost* host)
    : host_(host),
      ALLOW_THIS_IN_INITIALIZER_LIST(window_(new aura::Window(this))),
      is_fullscreen_(false),
      popup_parent_host_view_(NULL),
      is_loading_(false),
      text_input_type_(ui::TEXT_INPUT_TYPE_NONE),
      has_composition_text_(false),
#if defined(UI_COMPOSITOR_IMAGE_TRANSPORT)
      current_surface_(gfx::kNullPluginWindow),
#endif
      skip_schedule_paint_(false) {
  host_->SetView(this);
  aura::client::SetTooltipText(window_, &tooltip_);
  aura::client::SetActivationDelegate(window_, this);
}

RenderWidgetHostViewAura::~RenderWidgetHostViewAura() {
  aura::client::SetTooltipText(window_, NULL);
#if defined(UI_COMPOSITOR_IMAGE_TRANSPORT)
  ui::Compositor* compositor = window_->layer()->GetCompositor();
  if (compositor && compositor->HasObserver(this))
    compositor->RemoveObserver(this);
#endif
}

void RenderWidgetHostViewAura::InitAsChild() {
  window_->Init(ui::Layer::LAYER_HAS_TEXTURE);
  window_->SetName("RenderWidgetHostViewAura");
}

////////////////////////////////////////////////////////////////////////////////
// RenderWidgetHostViewAura, RenderWidgetHostView implementation:

void RenderWidgetHostViewAura::InitAsPopup(
    RenderWidgetHostView* parent_host_view,
    const gfx::Rect& pos) {
  popup_parent_host_view_ =
      static_cast<RenderWidgetHostViewAura*>(parent_host_view);
  window_->SetType(aura::client::WINDOW_TYPE_MENU);
  window_->Init(ui::Layer::LAYER_HAS_TEXTURE);
  window_->SetName("RenderWidgetHostViewAura");

  window_->SetParent(NULL);
  Show();

  // |pos| is in root window coordinates. So convert it to
  // |popup_parent_host_view_|'s coordinates first.
  gfx::Point origin = pos.origin();
  aura::Window::ConvertPointToWindow(
      aura::RootWindow::GetInstance(),
      popup_parent_host_view_->window_, &origin);
  SetBounds(gfx::Rect(origin, pos.size()));
}

void RenderWidgetHostViewAura::InitAsFullscreen(
    RenderWidgetHostView* reference_host_view) {
  is_fullscreen_ = true;
  window_->SetType(aura::client::WINDOW_TYPE_NORMAL);
  window_->Init(ui::Layer::LAYER_HAS_TEXTURE);
  window_->SetName("RenderWidgetHostViewAura");
  window_->SetIntProperty(aura::client::kShowStateKey,
                          ui::SHOW_STATE_FULLSCREEN);
  window_->SetParent(NULL);
  Show();
  Focus();
}

RenderWidgetHost* RenderWidgetHostViewAura::GetRenderWidgetHost() const {
  return host_;
}

void RenderWidgetHostViewAura::DidBecomeSelected() {
  host_->WasRestored();
}

void RenderWidgetHostViewAura::WasHidden() {
  host_->WasHidden();
}

void RenderWidgetHostViewAura::SetSize(const gfx::Size& size) {
  SetBounds(gfx::Rect(window_->bounds().origin(), size));
}

void RenderWidgetHostViewAura::SetBounds(const gfx::Rect& rect) {
  gfx::Rect adjusted_rect = rect;

  if (popup_parent_host_view_) {
    gfx::Point translated_origin = adjusted_rect.origin();
    // |rect| is relative to |popup_parent_host_view_|; translate it for the
    // window's container.
    aura::Window::ConvertPointToWindow(
        popup_parent_host_view_->window_,
        window_->parent(),
        &translated_origin);
    adjusted_rect.set_origin(translated_origin);
  }

  window_->SetBounds(adjusted_rect);
  host_->WasResized();
}

gfx::NativeView RenderWidgetHostViewAura::GetNativeView() const {
  return window_;
}

gfx::NativeViewId RenderWidgetHostViewAura::GetNativeViewId() const {
  return static_cast<gfx::NativeViewId>(NULL);
}

void RenderWidgetHostViewAura::MovePluginWindows(
    const std::vector<webkit::npapi::WebPluginGeometry>& moves) {
  // We don't support windowed plugins.
}

void RenderWidgetHostViewAura::Focus() {
  window_->Focus();
}

void RenderWidgetHostViewAura::Blur() {
  window_->Blur();
}

bool RenderWidgetHostViewAura::HasFocus() const {
  return window_->HasFocus();
}

void RenderWidgetHostViewAura::Show() {
  window_->Show();
}

void RenderWidgetHostViewAura::Hide() {
  window_->Hide();
}

bool RenderWidgetHostViewAura::IsShowing() {
  return window_->IsVisible();
}

gfx::Rect RenderWidgetHostViewAura::GetViewBounds() const {
  return window_->GetScreenBounds();
}

void RenderWidgetHostViewAura::UpdateCursor(const WebCursor& cursor) {
  current_cursor_ = cursor;
  UpdateCursorIfOverSelf();
}

void RenderWidgetHostViewAura::SetIsLoading(bool is_loading) {
  is_loading_ = is_loading;
  UpdateCursorIfOverSelf();
}

void RenderWidgetHostViewAura::TextInputStateChanged(
    ui::TextInputType type,
    bool can_compose_inline) {
  // TODO(kinaba): currently, can_compose_inline is ignored and always treated
  // as true. We need to support "can_compose_inline=false" for PPAPI plugins
  // that may want to avoid drawing composition-text by themselves and pass
  // the responsibility to the browser.
  if (text_input_type_ != type) {
    text_input_type_ = type;
    GetInputMethod()->OnTextInputTypeChanged(this);
  }
}

void RenderWidgetHostViewAura::ImeCancelComposition() {
  GetInputMethod()->CancelComposition(this);
  has_composition_text_ = false;
}

void RenderWidgetHostViewAura::DidUpdateBackingStore(
    const gfx::Rect& scroll_rect, int scroll_dx, int scroll_dy,
    const std::vector<gfx::Rect>& copy_rects) {
  if (!window_->IsVisible() || skip_schedule_paint_)
    return;

  if (!scroll_rect.IsEmpty())
    window_->SchedulePaintInRect(scroll_rect);

  for (size_t i = 0; i < copy_rects.size(); ++i) {
    gfx::Rect rect = copy_rects[i].Subtract(scroll_rect);
    if (rect.IsEmpty())
      continue;

    window_->SchedulePaintInRect(rect);
  }
}

void RenderWidgetHostViewAura::RenderViewGone(base::TerminationStatus status,
                                              int error_code) {
  UpdateCursorIfOverSelf();
  Destroy();
}

void RenderWidgetHostViewAura::Destroy() {
  delete window_;
}

void RenderWidgetHostViewAura::SetTooltipText(const string16& tooltip_text) {
  tooltip_ = tooltip_text;
  if (aura::client::GetTooltipClient())
    aura::client::GetTooltipClient()->UpdateTooltip(window_);
}

void RenderWidgetHostViewAura::SelectionBoundsChanged(
    const gfx::Rect& start_rect,
    const gfx::Rect& end_rect) {
  if (selection_start_rect_ == start_rect && selection_end_rect_ == end_rect)
    return;

  selection_start_rect_ = start_rect;
  selection_end_rect_ = end_rect;

  GetInputMethod()->OnCaretBoundsChanged(this);
}

BackingStore* RenderWidgetHostViewAura::AllocBackingStore(
    const gfx::Size& size) {
  return new BackingStoreSkia(host_, size);
}

void RenderWidgetHostViewAura::OnAcceleratedCompositingStateChange() {
  UpdateExternalTexture();
}

void RenderWidgetHostViewAura::UpdateExternalTexture() {
#if defined(UI_COMPOSITOR_IMAGE_TRANSPORT)
  if (current_surface_ != gfx::kNullPluginWindow &&
      host_->is_accelerated_compositing_active()) {
    window_->layer()->SetExternalTexture(
        accelerated_surface_containers_[current_surface_]->GetTexture());
    glFlush();
  } else {
    window_->layer()->SetExternalTexture(NULL);
  }
#endif
}

void RenderWidgetHostViewAura::AcceleratedSurfaceBuffersSwapped(
    const GpuHostMsg_AcceleratedSurfaceBuffersSwapped_Params& params,
    int gpu_host_id) {
#if defined(UI_COMPOSITOR_IMAGE_TRANSPORT)
  current_surface_ = params.surface_id;
  UpdateExternalTexture();

  if (!window_->layer()->GetCompositor()) {
    // We have no compositor, so we have no way to display the surface.
    // Must still send the ACK.
    RenderWidgetHost::AcknowledgeSwapBuffers(params.route_id, gpu_host_id);
  } else {
    gfx::Size surface_size =
        accelerated_surface_containers_[params.surface_id]->GetSize();
    window_->SchedulePaintInRect(gfx::Rect(surface_size));

    // Add sending an ACK to the list of things to do OnCompositingEnded
    on_compositing_ended_callbacks_.push_back(
        base::Bind(&RenderWidgetHost::AcknowledgeSwapBuffers,
                   params.route_id, gpu_host_id));
    ui::Compositor* compositor = window_->layer()->GetCompositor();
    if (!compositor->HasObserver(this))
      compositor->AddObserver(this);
  }
#else
  NOTREACHED();
#endif
}

void RenderWidgetHostViewAura::AcceleratedSurfacePostSubBuffer(
    const GpuHostMsg_AcceleratedSurfacePostSubBuffer_Params& params,
    int gpu_host_id) {
#if defined(UI_COMPOSITOR_IMAGE_TRANSPORT)
  current_surface_ = params.surface_id;
  UpdateExternalTexture();

  if (!window_->layer()->GetCompositor()) {
    // We have no compositor, so we have no way to display the surface
    // Must still send the ACK
    RenderWidgetHost::AcknowledgePostSubBuffer(params.route_id, gpu_host_id);
  } else {
    gfx::Size surface_size =
        accelerated_surface_containers_[params.surface_id]->GetSize();

    // Co-ordinates come in OpenGL co-ordinate space.
    // We need to convert to layer space.
    window_->SchedulePaintInRect(gfx::Rect(
        params.x,
        surface_size.height() - params.y - params.height,
        params.width,
        params.height));

    // Add sending an ACK to the list of things to do OnCompositingEnded
    on_compositing_ended_callbacks_.push_back(
        base::Bind(&RenderWidgetHost::AcknowledgePostSubBuffer,
                   params.route_id, gpu_host_id));
    ui::Compositor* compositor = window_->layer()->GetCompositor();
    if (!compositor->HasObserver(this))
      compositor->AddObserver(this);
  }
#else
  NOTREACHED();
#endif
}

#if defined(UI_COMPOSITOR_IMAGE_TRANSPORT)
void RenderWidgetHostViewAura::AcceleratedSurfaceNew(
      int32 width,
      int32 height,
      uint64* surface_id,
      TransportDIB::Handle* surface_handle) {
  scoped_refptr<AcceleratedSurfaceContainerLinux> surface(
      AcceleratedSurfaceContainerLinux::Create(gfx::Size(width, height)));
  if (!surface->Initialize(surface_id)) {
    LOG(ERROR) << "Failed to create AcceleratedSurfaceContainer";
    return;
  }
  *surface_handle = surface->Handle();

  accelerated_surface_containers_[*surface_id] = surface;
}

void RenderWidgetHostViewAura::AcceleratedSurfaceRelease(uint64 surface_id) {
  if (current_surface_ == surface_id) {
    current_surface_ = gfx::kNullPluginWindow;
    // Don't call UpdateExternalTexture: it's possible that a new surface with
    // the same ID will be re-created right away, in which case we don't want to
    // flip back and forth. Instead wait until we got the accelerated
    // compositing deactivation.
  }
  accelerated_surface_containers_.erase(surface_id);
}
#endif

void RenderWidgetHostViewAura::SetBackground(const SkBitmap& background) {
  RenderWidgetHostView::SetBackground(background);
  host_->SetBackground(background);
#if defined(UI_COMPOSITOR_IMAGE_TRANSPORT)
  window_->layer()->SetFillsBoundsOpaquely(background.isOpaque());
#endif
}

void RenderWidgetHostViewAura::GetScreenInfo(WebKit::WebScreenInfo* results) {
  GetDefaultScreenInfo(results);
}

gfx::Rect RenderWidgetHostViewAura::GetRootWindowBounds() {
  // TODO(beng): this is actually wrong, we are supposed to return the bounds
  //             of the container "top level" window, but we don't have a firm
  //             concept of what constitutes a toplevel right now, so just do
  //             this.
  return window_->GetScreenBounds();
}

void RenderWidgetHostViewAura::UnhandledWheelEvent(
    const WebKit::WebMouseWheelEvent& event) {
  // Not needed. Mac-only.
}

void RenderWidgetHostViewAura::SetHasHorizontalScrollbar(
    bool has_horizontal_scrollbar) {
  // Not needed. Mac-only.
}

void RenderWidgetHostViewAura::SetScrollOffsetPinning(
    bool is_pinned_to_left, bool is_pinned_to_right) {
  // Not needed. Mac-only.
}

#if defined(UI_COMPOSITOR_IMAGE_TRANSPORT)
gfx::PluginWindowHandle RenderWidgetHostViewAura::GetCompositingSurface() {
  // The GPU process renders to an offscreen surface (created by the GPU
  // process), which is later displayed by the browser. As the GPU process
  // creates this surface, we can return any non-zero value.
  return 1;
}
#else
gfx::PluginWindowHandle RenderWidgetHostViewAura::GetCompositingSurface() {
  // TODO(oshima): The original implementation was broken as
  // GtkNativeViewManager doesn't know about NativeWidgetGtk. Figure
  // out if this makes sense without compositor. If it does, then find
  // out the right way to handle.
  NOTIMPLEMENTED();
  return gfx::kNullPluginWindow;
}
#endif

bool RenderWidgetHostViewAura::LockMouse() {
  // http://crbug.com/102563
  NOTIMPLEMENTED();
  return false;
}

void RenderWidgetHostViewAura::UnlockMouse() {
  // http://crbug.com/102563
  NOTIMPLEMENTED();
  host_->LostMouseLock();
}

////////////////////////////////////////////////////////////////////////////////
// RenderWidgetHostViewAura, ui::TextInputClient implementation:
void RenderWidgetHostViewAura::SetCompositionText(
    const ui::CompositionText& composition) {
  if (!host_)
    return;

  // ui::CompositionUnderline should be identical to
  // WebKit::WebCompositionUnderline, so that we can do reinterpret_cast safely.
  COMPILE_ASSERT(sizeof(ui::CompositionUnderline) ==
                 sizeof(WebKit::WebCompositionUnderline),
                 ui_CompositionUnderline__WebKit_WebCompositionUnderline_diff);

  // TODO(suzhe): convert both renderer_host and renderer to use
  // ui::CompositionText.
  const std::vector<WebKit::WebCompositionUnderline>& underlines =
      reinterpret_cast<const std::vector<WebKit::WebCompositionUnderline>&>(
          composition.underlines);

  // TODO(suzhe): due to a bug of webkit, we can't use selection range with
  // composition string. See: https://bugs.webkit.org/show_bug.cgi?id=37788
  host_->ImeSetComposition(composition.text, underlines,
                           composition.selection.end(),
                           composition.selection.end());

  has_composition_text_ = !composition.text.empty();
}

void RenderWidgetHostViewAura::ConfirmCompositionText() {
  if (host_ && has_composition_text_)
    host_->ImeConfirmComposition();
  has_composition_text_ = false;
}

void RenderWidgetHostViewAura::ClearCompositionText() {
  if (host_ && has_composition_text_)
    host_->ImeCancelComposition();
  has_composition_text_ = false;
}

void RenderWidgetHostViewAura::InsertText(const string16& text) {
  DCHECK(text_input_type_ != ui::TEXT_INPUT_TYPE_NONE);
  if (host_)
    host_->ImeConfirmComposition(text);
  has_composition_text_ = false;
}

void RenderWidgetHostViewAura::InsertChar(char16 ch, int flags) {
  if (host_) {
    // Send a WebKit::WebInputEvent::Char event to |host_|.
    NativeWebKeyboardEvent webkit_event(ui::ET_KEY_PRESSED,
                                        true /* is_char */,
                                        ch,
                                        flags,
                                        base::Time::Now().ToDoubleT());
    host_->ForwardKeyboardEvent(webkit_event);
  }
}

ui::TextInputType RenderWidgetHostViewAura::GetTextInputType() const {
  return text_input_type_;
}

gfx::Rect RenderWidgetHostViewAura::GetCaretBounds() {
  const gfx::Rect rect = selection_start_rect_.Union(selection_end_rect_);
  gfx::Point origin = rect.origin();
  gfx::Point end = gfx::Point(rect.right(), rect.bottom());

  aura::RootWindow* root_window = aura::RootWindow::GetInstance();
  aura::Window::ConvertPointToWindow(window_, root_window, &origin);
  aura::Window::ConvertPointToWindow(window_, root_window, &end);
  // TODO(yusukes): Unlike Chrome OS, |root_window| origin might not be the
  // same as the system screen origin on Windows and Linux. Probably we should
  // (implement and) use something like ConvertPointToScreen().

  return gfx::Rect(origin.x(),
                   origin.y(),
                   end.x() - origin.x(),
                   end.y() - origin.y());
}

bool RenderWidgetHostViewAura::HasCompositionText() {
  return has_composition_text_;
}

bool RenderWidgetHostViewAura::GetTextRange(ui::Range* range) {
  range->set_start(selection_text_offset_);
  range->set_end(selection_text_offset_ + selection_text_.length());
  return true;
}

bool RenderWidgetHostViewAura::GetCompositionTextRange(ui::Range* range) {
  // TODO(suzhe): implement this method when fixing http://crbug.com/55130.
  NOTIMPLEMENTED();
  return false;
}

bool RenderWidgetHostViewAura::GetSelectionRange(ui::Range* range) {
  range->set_start(selection_range_.start());
  range->set_end(selection_range_.end());
  return true;
}

bool RenderWidgetHostViewAura::SetSelectionRange(const ui::Range& range) {
  // TODO(suzhe): implement this method when fixing http://crbug.com/55130.
  NOTIMPLEMENTED();
  return false;
}

bool RenderWidgetHostViewAura::DeleteRange(const ui::Range& range) {
  // TODO(suzhe): implement this method when fixing http://crbug.com/55130.
  NOTIMPLEMENTED();
  return false;
}

bool RenderWidgetHostViewAura::GetTextFromRange(
    const ui::Range& range,
    string16* text) {
  ui::Range selection_text_range(selection_text_offset_,
      selection_text_offset_ + selection_text_.length());

  if (!selection_text_range.Contains(range)) {
    text->clear();
    return false;
  }
  if (selection_text_range.EqualsIgnoringDirection(range)) {
    // Avoid calling substr whose performance is low.
    *text = selection_text_;
  } else {
    *text = selection_text_.substr(
        range.GetMin() - selection_text_offset_,
        range.length());
  }
  return true;
}

void RenderWidgetHostViewAura::OnInputMethodChanged() {
  if (!host_)
    return;

  host_->SetInputMethodActive(GetInputMethod()->IsActive());

  // TODO(suzhe): implement the newly added “locale” property of HTML DOM
  // TextEvent.
}

bool RenderWidgetHostViewAura::ChangeTextDirectionAndLayoutAlignment(
      base::i18n::TextDirection direction) {
  if (!host_)
    return false;
  host_->UpdateTextDirection(
      direction == base::i18n::RIGHT_TO_LEFT ?
      WebKit::WebTextDirectionRightToLeft :
      WebKit::WebTextDirectionLeftToRight);
  host_->NotifyTextDirection();
  return true;
}

////////////////////////////////////////////////////////////////////////////////
// RenderWidgetHostViewAura, aura::WindowDelegate implementation:

gfx::Size RenderWidgetHostViewAura::GetMinimumSize() const {
  return gfx::Size();
}

void RenderWidgetHostViewAura::OnBoundsChanged(const gfx::Rect& old_bounds,
                                               const gfx::Rect& new_bounds) {
  // We don't care about this one, we are always sized via SetSize() or
  // SetBounds().
}

void RenderWidgetHostViewAura::OnFocus() {
  host_->GotFocus();
  host_->SetActive(true);

  ui::InputMethod* input_method = GetInputMethod();
  if (input_method) {
    // Ask the system-wide IME to send all TextInputClient messages to |this|
    // object.
    input_method->SetFocusedTextInputClient(this);
    host_->SetInputMethodActive(input_method->IsActive());
  } else {
    host_->SetInputMethodActive(false);
  }
}

void RenderWidgetHostViewAura::OnBlur() {
  host_->SetActive(false);
  host_->Blur();

  ui::InputMethod* input_method = GetInputMethod();
  if (input_method) {
    if (input_method->GetTextInputClient() == this)
      input_method->SetFocusedTextInputClient(NULL);
  }
  host_->SetInputMethodActive(false);
}

bool RenderWidgetHostViewAura::OnKeyEvent(aura::KeyEvent* event) {
  // We need to handle the Escape key for Pepper Flash.
  if (is_fullscreen_ && event->key_code() == ui::VKEY_ESCAPE) {
    host_->Shutdown();
  } else {
    // We don't have to communicate with an input method here.
    if (!event->HasNativeEvent()) {
      // Send a fabricated event, which is usually a VKEY_PROCESSKEY IME event.
      NativeWebKeyboardEvent webkit_event(event->type(),
                                          false /* is_char */,
                                          event->GetCharacter(),
                                          event->flags(),
                                          base::Time::Now().ToDoubleT());
      host_->ForwardKeyboardEvent(webkit_event);
    } else {
      NativeWebKeyboardEvent webkit_event(event);
      host_->ForwardKeyboardEvent(webkit_event);
    }
  }
  return true;
}

gfx::NativeCursor RenderWidgetHostViewAura::GetCursor(const gfx::Point& point) {
  return current_cursor_.GetNativeCursor();
}

int RenderWidgetHostViewAura::GetNonClientComponent(
    const gfx::Point& point) const {
  return HTCLIENT;
}

bool RenderWidgetHostViewAura::OnMouseEvent(aura::MouseEvent* event) {
  if (event->type() == ui::ET_MOUSEWHEEL) {
    WebKit::WebMouseWheelEvent mouse_wheel_event =
        content::MakeWebMouseWheelEvent(event);
    if (mouse_wheel_event.deltaX != 0)
      host_->ForwardWheelEvent(mouse_wheel_event);
  } else if (event->type() == ui::ET_SCROLL) {
    WebKit::WebMouseWheelEvent mouse_wheel_event =
        content::MakeWebMouseWheelEvent(static_cast<aura::ScrollEvent*>(event));
    if (mouse_wheel_event.deltaX != 0)
      host_->ForwardWheelEvent(mouse_wheel_event);
  } else if (CanRendererHandleEvent(event->native_event())) {
    host_->ForwardMouseEvent(content::MakeWebMouseEvent(event));
  }

  switch (event->type()) {
    case ui::ET_MOUSE_PRESSED:
      window_->SetCapture();
      // Confirm existing composition text on mouse click events, to make sure
      // the input caret won't be moved with an ongoing composition text.
      FinishImeCompositionSession();
      break;
    case ui::ET_MOUSE_RELEASED:
      window_->ReleaseCapture();
      break;
    default:
      break;
  }

  // Needed to propagate mouse event to native_tab_contents_view_aura.
  // TODO(pkotwicz): Find a better way of doing this.
  if (window_->parent()->delegate())
    window_->parent()->delegate()->OnMouseEvent(event);

  // Return true so that we receive released/drag events.
  return true;
}

ui::TouchStatus RenderWidgetHostViewAura::OnTouchEvent(
    aura::TouchEvent* event) {
  // Update the touch event first.
  WebKit::WebTouchPoint* point = content::UpdateWebTouchEvent(event,
      &touch_event_);

  // Forward the touch event only if a touch point was updated.
  if (point) {
    host_->ForwardTouchEvent(touch_event_);
    UpdateWebTouchEventAfterDispatch(&touch_event_, point);
  }

  return DecideTouchStatus(touch_event_, point);
}

bool RenderWidgetHostViewAura::CanFocus() {
  return popup_type_ == WebKit::WebPopupTypeNone;
}

void RenderWidgetHostViewAura::OnCaptureLost() {
  host_->LostCapture();
}

void RenderWidgetHostViewAura::OnPaint(gfx::Canvas* canvas) {
  if (!window_->IsVisible())
    return;
  skip_schedule_paint_ = true;
  BackingStore* backing_store = host_->GetBackingStore(true);
  skip_schedule_paint_ = false;
  if (backing_store) {
    static_cast<BackingStoreSkia*>(backing_store)->SkiaShowRect(gfx::Point(),
                                                                canvas);
  } else {
    canvas->FillRect(SK_ColorWHITE,
                     gfx::Rect(gfx::Point(), window_->bounds().size()));
  }
}

void RenderWidgetHostViewAura::OnWindowDestroying() {
}

void RenderWidgetHostViewAura::OnWindowDestroyed() {
  host_->ViewDestroyed();
  delete this;
}

void RenderWidgetHostViewAura::OnWindowVisibilityChanged(bool visible) {
}

////////////////////////////////////////////////////////////////////////////////
// RenderWidgetHostViewAura, aura::client::ActivationDelegate implementation:

bool RenderWidgetHostViewAura::ShouldActivate(aura::Event* event) {
  return is_fullscreen_;
}

void RenderWidgetHostViewAura::OnActivated() {
}

void RenderWidgetHostViewAura::OnLostActive() {
}

#if defined(UI_COMPOSITOR_IMAGE_TRANSPORT)
////////////////////////////////////////////////////////////////////////////////
// RenderWidgetHostViewAura, ui::CompositorDelegate implementation:

void RenderWidgetHostViewAura::OnCompositingEnded(ui::Compositor* compositor) {
  for (std::vector< base::Callback<void(void)> >::const_iterator
      it = on_compositing_ended_callbacks_.begin();
      it != on_compositing_ended_callbacks_.end(); ++it) {
    it->Run();
  }
  on_compositing_ended_callbacks_.clear();
  compositor->RemoveObserver(this);
}
#endif

////////////////////////////////////////////////////////////////////////////////
// RenderWidgetHostViewAura, private:

void RenderWidgetHostViewAura::UpdateCursorIfOverSelf() {
  const gfx::Point screen_point = gfx::Screen::GetCursorScreenPoint();
  aura::RootWindow* root_window = aura::RootWindow::GetInstance();
  if (root_window->GetEventHandlerForPoint(screen_point) != window_)
    return;

  gfx::NativeCursor cursor = current_cursor_.GetNativeCursor();
  if (is_loading_ && cursor == aura::kCursorPointer)
    cursor = aura::kCursorProgress;

  aura::RootWindow::GetInstance()->SetCursor(cursor);
}

ui::InputMethod* RenderWidgetHostViewAura::GetInputMethod() const {
  aura::RootWindow* root_window = aura::RootWindow::GetInstance();
  return reinterpret_cast<ui::InputMethod*>(
      root_window->GetProperty(aura::client::kRootWindowInputMethod));
}

void RenderWidgetHostViewAura::FinishImeCompositionSession() {
  if (!has_composition_text_)
    return;
  if (host_)
    host_->ImeConfirmComposition();
  ImeCancelComposition();
}

////////////////////////////////////////////////////////////////////////////////
// RenderWidgetHostView, public:

// static
RenderWidgetHostView* RenderWidgetHostView::CreateViewForWidget(
    RenderWidgetHost* widget) {
  return new RenderWidgetHostViewAura(widget);
}

// static
void RenderWidgetHostView::GetDefaultScreenInfo(
    WebKit::WebScreenInfo* results) {
  const gfx::Size size = gfx::Screen::GetPrimaryMonitorSize();
  results->rect = WebKit::WebRect(0, 0, size.width(), size.height());
  results->availableRect = results->rect;
  // TODO(derat): Don't hardcode this?
  results->depth = 24;
  results->depthPerComponent = 8;
}
