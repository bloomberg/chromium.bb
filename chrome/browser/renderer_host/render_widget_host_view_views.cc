// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/render_widget_host_view_views.h"

#include <algorithm>
#include <string>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/string_number_conversions.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/native_web_keyboard_event_views.h"
#include "chrome/common/render_messages.h"
#include "content/browser/renderer_host/backing_store_skia.h"
#include "content/browser/renderer_host/render_widget_host.h"
#include "content/common/view_messages.h"
#include "content/public/browser/notification_service.h"
#include "content/public/common/result_codes.h"
#include "grit/ui_strings.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebInputEvent.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/gtk/WebInputEventFactory.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/x11/WebScreenInfoFactory.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/text/text_elider.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/canvas_skia.h"
#include "views/events/event.h"
#include "views/ime/input_method.h"
#include "views/views_delegate.h"
#include "views/widget/tooltip_manager.h"
#include "views/widget/widget.h"

#if defined(UI_COMPOSITOR_IMAGE_TRANSPORT)
#include "base/bind.h"
#include "content/browser/gpu/gpu_process_host_ui_shim.h"
#include "content/browser/renderer_host/accelerated_surface_container_linux.h"
#include "content/common/gpu/gpu_messages.h"
#include "ui/gfx/gl/gl_bindings.h"
#endif

#if defined(TOOLKIT_USES_GTK)
#include <gtk/gtk.h>
#include <gtk/gtkwindow.h>
#include <gdk/gdkx.h>
#include "content/browser/renderer_host/gtk_window_utils.h"
#include "views/widget/native_widget_gtk.h"
#endif

#if defined(OS_POSIX)
#include "content/browser/renderer_host/gtk_window_utils.h"
#endif

static const int kMaxWindowWidth = 4000;
static const int kMaxWindowHeight = 4000;
static const int kTouchControllerUpdateDelay = 150;

// static
const char RenderWidgetHostViewViews::kViewClassName[] =
    "browser/renderer_host/RenderWidgetHostViewViews";

using WebKit::WebInputEventFactory;
using WebKit::WebMouseWheelEvent;
using WebKit::WebTouchEvent;

namespace {

int WebInputEventFlagsFromViewsEvent(const views::Event& event) {
  int modifiers = 0;

  if (event.IsShiftDown())
    modifiers |= WebKit::WebInputEvent::ShiftKey;
  if (event.IsControlDown())
    modifiers |= WebKit::WebInputEvent::ControlKey;
  if (event.IsAltDown())
    modifiers |= WebKit::WebInputEvent::AltKey;
  if (event.IsCapsLockDown())
    modifiers |= WebKit::WebInputEvent::CapsLockOn;

  return modifiers;
}

void InitializeWebMouseEventFromViewsEvent(const views::LocatedEvent& event,
                                           const gfx::Point& origin,
                                           WebKit::WebMouseEvent* wmevent) {
  wmevent->timeStampSeconds = base::Time::Now().ToDoubleT();
  wmevent->modifiers = WebInputEventFlagsFromViewsEvent(event);

  wmevent->windowX = wmevent->x = event.x();
  wmevent->windowY = wmevent->y = event.y();
  wmevent->globalX = wmevent->x + origin.x();
  wmevent->globalY = wmevent->y + origin.y();
}

#if defined(UI_COMPOSITOR_IMAGE_TRANSPORT)
void AcknowledgeSwapBuffers(int32 route_id, int gpu_host_id) {
  // It's possible that gpu_host_id is no longer valid at this point (like if
  // gpu process was restarted after a crash).  SendToGpuHost handles this.
  GpuProcessHostUIShim::SendToGpuHost(gpu_host_id,
      new AcceleratedSurfaceMsg_BuffersSwappedACK(route_id));
}
#endif

}  // namespace

RenderWidgetHostViewViews::RenderWidgetHostViewViews(RenderWidgetHost* host)
    : host_(host),
      about_to_validate_and_paint_(false),
      is_hidden_(false),
      is_loading_(false),
      native_cursor_(gfx::kNullCursor),
      is_showing_context_menu_(false),
      visually_deemphasized_(false),
      touch_event_(),
      text_input_type_(ui::TEXT_INPUT_TYPE_NONE),
      has_composition_text_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(touch_selection_controller_(
          views::TouchSelectionController::create(this))),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)) {
  set_focusable(true);
  host_->SetView(this);

#if defined(TOUCH_UI)
  SetPaintToLayer(true);
  registrar_.Add(this,
                 chrome::NOTIFICATION_KEYBOARD_VISIBLE_BOUNDS_CHANGED,
                 content::NotificationService::AllSources());
#endif
}

RenderWidgetHostViewViews::~RenderWidgetHostViewViews() {
}

void RenderWidgetHostViewViews::InitAsChild() {
  Show();
}

void RenderWidgetHostViewViews::InitAsPopup(
    RenderWidgetHostView* parent_host_view,
    const gfx::Rect& pos) {
  RenderWidgetHostViewViews* parent =
      static_cast<RenderWidgetHostViewViews*>(parent_host_view);
  parent->AddChildView(this);
  // If the parent loses focus then the popup will close. So we need
  // to tell the parent it's showing a popup so that it doesn't respond to
  // blurs.
  parent->is_showing_context_menu_ = true;

  // pos is in screen coordinates but a view is positioned relative
  // to its parent. Here we convert screen coordinates to relative
  // coordinates.
  gfx::Point p(pos.x() - parent_host_view->GetViewBounds().x(),
               pos.y() - parent_host_view->GetViewBounds().y());
  views::View::SetBounds(p.x(), p.y(), pos.width(), pos.height());
  Show();

  if (NeedsInputGrab()) {
    set_focusable(true);
    RequestFocus();
  }
}

void RenderWidgetHostViewViews::InitAsFullscreen(
    RenderWidgetHostView* /*reference_host_view*/) {
  NOTIMPLEMENTED();
}

RenderWidgetHost* RenderWidgetHostViewViews::GetRenderWidgetHost() const {
  return host_;
}

void RenderWidgetHostViewViews::DidBecomeSelected() {
  if (!is_hidden_)
    return;

  if (tab_switch_paint_time_.is_null())
    tab_switch_paint_time_ = base::TimeTicks::Now();
  is_hidden_ = false;
  if (host_)
    host_->WasRestored();
}

void RenderWidgetHostViewViews::WasHidden() {
  if (is_hidden_)
    return;

  // If we receive any more paint messages while we are hidden, we want to
  // ignore them so we don't re-allocate the backing store.  We will paint
  // everything again when we become selected again.
  is_hidden_ = true;

  // If we have a renderer, then inform it that we are being hidden so it can
  // reduce its resource utilization.
  if (host_)
    host_->WasHidden();

  weak_factory_.InvalidateWeakPtrs();
}

void RenderWidgetHostViewViews::SetSize(const gfx::Size& size) {
  // This is called when webkit has sent us a Move message.
  int width = std::min(size.width(), kMaxWindowWidth);
  int height = std::min(size.height(), kMaxWindowHeight);
  if (requested_size_.width() != width ||
      requested_size_.height() != height) {
    requested_size_ = gfx::Size(width, height);
    views::View::SetBounds(x(), y(), width, height);
    if (host_)
      host_->WasResized();
  }
}

void RenderWidgetHostViewViews::SetBounds(const gfx::Rect& rect) {
  // TODO(oshima): chromeos/touch doesn't allow moving window.
  SetSize(rect.size());
}

gfx::NativeView RenderWidgetHostViewViews::GetNativeView() const {
  // TODO(oshima): There is no native view here for RWHVV.
  // Use top level widget's native view for now. This is not
  // correct and has to be fixed somehow.
  return GetWidget() ? GetWidget()->GetNativeView() : NULL;
}

gfx::NativeViewId RenderWidgetHostViewViews::GetNativeViewId() const {
#if defined(OS_WIN)
  // TODO(oshima): Windows uses message filter to handle inquiry for
  // window/screen info, which requires HWND. Just pass the
  // browser window's HWND (same as before) for now.
  return reinterpret_cast<gfx::NativeViewId>(GetNativeView());
#else
  return reinterpret_cast<gfx::NativeViewId>(
      const_cast<RenderWidgetHostViewViews*>(this));
#endif
}

void RenderWidgetHostViewViews::MovePluginWindows(
    const std::vector<webkit::npapi::WebPluginGeometry>& moves) {
  // TODO(anicolao): NIY
  // NOTIMPLEMENTED();
}

bool RenderWidgetHostViewViews::HasFocus() const {
  return View::HasFocus();
}

void RenderWidgetHostViewViews::Show() {
  SetVisible(true);
}

void RenderWidgetHostViewViews::Hide() {
  SetVisible(false);
}

bool RenderWidgetHostViewViews::IsShowing() {
  return IsVisible();
}

gfx::Rect RenderWidgetHostViewViews::GetViewBounds() const {
  return GetScreenBounds();
}

void RenderWidgetHostViewViews::SetIsLoading(bool is_loading) {
  is_loading_ = is_loading;
#if defined(TOOLKIT_USES_GTK)
  // Only call ShowCurrentCursor() when it will actually change the cursor.
  if (current_cursor_.GetCursorType() == GDK_LAST_CURSOR)
    ShowCurrentCursor();
#endif  // TOOLKIT_USES_GTK
}

void RenderWidgetHostViewViews::TextInputStateChanged(
    ui::TextInputType type,
    bool can_compose_inline) {
  // TODO(kinaba): currently, can_compose_inline is ignored and always treated
  // as true. We need to support "can_compose_inline=false" for PPAPI plugins
  // that may want to avoid drawing composition-text by themselves and pass
  // the responsibility to the browser.

  // This is async message and by the time the ipc arrived,
  // RWHVV may be detached from Top level widget, in which case
  // GetInputMethod may return NULL;
  views::InputMethod* input_method = GetInputMethod();

  if (text_input_type_ != type) {
    text_input_type_ = type;
    if (input_method)
      input_method->OnTextInputTypeChanged(this);
  }
}

void RenderWidgetHostViewViews::ImeCancelComposition() {
  DCHECK(GetInputMethod());
  GetInputMethod()->CancelComposition(this);
  has_composition_text_ = false;
}

void RenderWidgetHostViewViews::DidUpdateBackingStore(
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
    SchedulePaintInRect(scroll_rect);

  for (size_t i = 0; i < copy_rects.size(); ++i) {
    // Avoid double painting.  NOTE: This is only relevant given the call to
    // Paint(scroll_rect) above.
    gfx::Rect rect = copy_rects[i].Subtract(scroll_rect);
    if (rect.IsEmpty())
      continue;

    if (about_to_validate_and_paint_)
      invalid_rect_ = invalid_rect_.Union(rect);
    else
      SchedulePaintInRect(rect);
  }
  invalid_rect_ = invalid_rect_.Intersect(bounds());
}

void RenderWidgetHostViewViews::RenderViewGone(base::TerminationStatus status,
                                               int error_code) {
  DCHECK(host_);
  host_->ViewDestroyed();
  Destroy();
}

void RenderWidgetHostViewViews::Destroy() {
  // host_'s destruction brought us here, null it out so we don't use it
  host_ = NULL;
  if (parent()) {
    if (IsPopup()) {
      static_cast<RenderWidgetHostViewViews*>
          (parent())->is_showing_context_menu_ = false;
      // We're hiding the popup so we need to make sure we repaint
      // what's underneath.
      parent()->SchedulePaintInRect(bounds());
    }
    parent()->RemoveChildView(this);
  }
  MessageLoop::current()->DeleteSoon(FROM_HERE, this);

#if defined(UI_COMPOSITOR_IMAGE_TRANSPORT)
  // Send out all outstanding ACKs so that we don't block the GPU
  // process.
  for (std::vector< base::Callback<void(void)> >::const_iterator
      it = on_compositing_ended_callbacks_.begin();
      it != on_compositing_ended_callbacks_.end(); ++it) {
    it->Run();
  }
  on_compositing_ended_callbacks_.clear();

  // Remove dangling reference.
  if (GetWidget() && GetWidget()->GetCompositor()) {
    ui::Compositor *compositor = GetWidget()->GetCompositor();
    if (compositor->HasObserver(this))
      compositor->RemoveObserver(this);
  }
#endif
}

void RenderWidgetHostViewViews::SetTooltipText(const string16& tip) {
  const int kMaxTooltipLength = 8 << 10;
  // Clamp the tooltip length to kMaxTooltipLength so that we don't
  // accidentally DOS the user with a mega tooltip.
  tooltip_text_ = ui::TruncateString(tip, kMaxTooltipLength);
  if (GetWidget())
    GetWidget()->TooltipTextChanged(this);
}

void RenderWidgetHostViewViews::SelectionChanged(const string16& text,
                                                 size_t offset,
                                                 const ui::Range& range) {
  RenderWidgetHostView::SelectionChanged(text, offset, range);
  // TODO(anicolao): deal with the clipboard without GTK
  NOTIMPLEMENTED();
}

void RenderWidgetHostViewViews::SelectionBoundsChanged(
    const gfx::Rect& start_rect,
    const gfx::Rect& end_rect) {
  views::InputMethod* input_method = GetInputMethod();

  if (selection_start_rect_ == start_rect && selection_end_rect_ == end_rect)
    return;

  selection_start_rect_ = start_rect;
  selection_end_rect_ = end_rect;

  if (input_method)
    input_method->OnCaretBoundsChanged(this);

  // TODO(sad): This is a workaround for a webkit bug:
  // https://bugs.webkit.org/show_bug.cgi?id=67464
  // Remove this when the bug gets fixed.
  //
  // Webkit can send spurious selection-change on text-input (e.g. when
  // inserting text at the beginning of a non-empty text control). But in those
  // cases, it does send the correct selection information quickly afterwards.
  // So delay the notification to the touch-selection controller.
  if (!weak_factory_.HasWeakPtrs()) {
    MessageLoop::current()->PostDelayedTask(FROM_HERE,
        base::Bind(
          &RenderWidgetHostViewViews::UpdateTouchSelectionController,
          weak_factory_.GetWeakPtr()),
        kTouchControllerUpdateDelay);
  }
}

void RenderWidgetHostViewViews::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
#if defined(TOUCH_UI)
  if (type != chrome::NOTIFICATION_KEYBOARD_VISIBLE_BOUNDS_CHANGED) {
    NOTREACHED();
    return;
  }

  if (!GetWidget())
    return;

  gfx::Rect keyboard_rect = *content::Details<gfx::Rect>(details).ptr();
  if (keyboard_rect != keyboard_rect_) {
    keyboard_rect_ = keyboard_rect;
    gfx::Rect screen_bounds = GetScreenBounds();
    gfx::Rect intersecting_rect = screen_bounds.Intersect(keyboard_rect);
    gfx::Rect available_rect = screen_bounds.Subtract(intersecting_rect);

    // Convert available rect to window (RWHVV) coordinates.
    gfx::Point origin = available_rect.origin();
    gfx::Rect r = GetWidget()->GetClientAreaScreenBounds();
    origin.SetPoint(origin.x() - r.x(), origin.y() - r.y());
    views::View::ConvertPointFromWidget(this, &origin);
    available_rect.set_origin(origin);
    if (!available_rect.IsEmpty())
      host_->ScrollFocusedEditableNodeIntoRect(available_rect);
  }
#endif
}

void RenderWidgetHostViewViews::ShowingContextMenu(bool showing) {
  is_showing_context_menu_ = showing;
}

BackingStore* RenderWidgetHostViewViews::AllocBackingStore(
    const gfx::Size& size) {
  return new BackingStoreSkia(host_, size);
}

void RenderWidgetHostViewViews::OnAcceleratedCompositingStateChange() {
#if defined(TOOLKIT_USES_GTK)
  bool activated = host_->is_accelerated_compositing_active();
#if defined(UI_COMPOSITOR_IMAGE_TRANSPORT)
  // If we don't use a views compositor, we currently have no way of
  // supporting rendering via the GPU process.
  if (!get_use_acceleration_when_possible() && activated)
    NOTREACHED();
#else
  if (activated)
    NOTIMPLEMENTED();
#endif
#endif
}

void RenderWidgetHostViewViews::SetBackground(const SkBitmap& background) {
  RenderWidgetHostView::SetBackground(background);
  if (host_)
    host_->SetBackground(background);
}

void RenderWidgetHostViewViews::SetVisuallyDeemphasized(
    const SkColor* color, bool animate) {
  // TODO(anicolao)
}

void RenderWidgetHostViewViews::UnhandledWheelEvent(
    const WebKit::WebMouseWheelEvent& event) {
}

void RenderWidgetHostViewViews::SetHasHorizontalScrollbar(
    bool has_horizontal_scrollbar) {
}

void RenderWidgetHostViewViews::SetScrollOffsetPinning(
    bool is_pinned_to_left, bool is_pinned_to_right) {
}

bool RenderWidgetHostViewViews::LockMouse() {
  NOTIMPLEMENTED();
  return false;
}

void RenderWidgetHostViewViews::UnlockMouse() {
  NOTIMPLEMENTED();
}

void RenderWidgetHostViewViews::SelectRect(const gfx::Point& start,
                                           const gfx::Point& end) {
  if (host_)
    host_->SelectRange(start, end);
}

bool RenderWidgetHostViewViews::IsCommandIdChecked(int command_id) const {
  NOTREACHED();
  return true;
}

bool RenderWidgetHostViewViews::IsCommandIdEnabled(int command_id) const {
  bool editable = GetTextInputType() != ui::TEXT_INPUT_TYPE_NONE;
  bool has_selection = !selection_range_.is_empty();
  string16 result;
  switch (command_id) {
    case IDS_APP_CUT:
      return editable && has_selection;
    case IDS_APP_COPY:
      return has_selection;
    case IDS_APP_PASTE:
      views::ViewsDelegate::views_delegate->GetClipboard()->
          ReadText(ui::Clipboard::BUFFER_STANDARD, &result);
      return editable && !result.empty();
    case IDS_APP_DELETE:
      return editable && has_selection;
    case IDS_APP_SELECT_ALL:
      return true;
    default:
      NOTREACHED();
      return false;
  }
  return true;
}

bool RenderWidgetHostViewViews::GetAcceleratorForCommandId(
    int command_id,
    ui::Accelerator* accelerator) {
  NOTREACHED();
  return true;
}

void RenderWidgetHostViewViews::ExecuteCommand(int command_id) {
  switch (command_id) {
    case IDS_APP_CUT:
      host_->Cut();
      break;
    case IDS_APP_COPY:
      host_->Copy();
      break;
    case IDS_APP_PASTE:
      host_->Paste();
      break;
    case IDS_APP_DELETE:
      host_->Delete();
      break;
    case IDS_APP_SELECT_ALL:
      host_->SelectAll();
      break;
    default:
      NOTREACHED();
  }
}

std::string RenderWidgetHostViewViews::GetClassName() const {
  return kViewClassName;
}

gfx::NativeCursor RenderWidgetHostViewViews::GetCursor(
    const views::MouseEvent& event) {
  return native_cursor_;
}

bool RenderWidgetHostViewViews::OnMousePressed(const views::MouseEvent& event) {
  // The special buttons on a mouse (e.g. back, forward etc.) are not correctly
  // recognized by views Events. Ignore those events here, so that the event
  // bubbles up the view hierarchy and the appropriate parent view can handle
  // them.
  if (!(event.flags() & (ui::EF_LEFT_BUTTON_DOWN |
                         ui::EF_RIGHT_BUTTON_DOWN |
                         ui::EF_MIDDLE_BUTTON_DOWN))) {
    return false;
  }

  if (!host_)
    return false;

  RequestFocus();

  // Confirm existing composition text on mouse click events, to make sure
  // the input caret won't be moved with an ongoing composition text.
  FinishImeCompositionSession();

  // TODO(anicolao): validate event generation.
  WebKit::WebMouseEvent e = WebMouseEventFromViewsEvent(event);

  // TODO(anicolao): deal with double clicks
  e.type = WebKit::WebInputEvent::MouseDown;
  e.clickCount = 1;

  host_->ForwardMouseEvent(e);
  return true;
}

bool RenderWidgetHostViewViews::OnMouseDragged(const views::MouseEvent& event) {
  OnMouseMoved(event);
  return true;
}

void RenderWidgetHostViewViews::OnMouseReleased(
    const views::MouseEvent& event) {
  if (!(event.flags() & (ui::EF_LEFT_BUTTON_DOWN |
                         ui::EF_RIGHT_BUTTON_DOWN |
                         ui::EF_MIDDLE_BUTTON_DOWN))) {
    return;
  }

  if (!host_)
    return;

  WebKit::WebMouseEvent e = WebMouseEventFromViewsEvent(event);
  e.type = WebKit::WebInputEvent::MouseUp;
  e.clickCount = 1;
  host_->ForwardMouseEvent(e);
}

void RenderWidgetHostViewViews::OnMouseMoved(const views::MouseEvent& event) {
  if (!host_)
    return;

  WebKit::WebMouseEvent e = WebMouseEventFromViewsEvent(event);
  e.type = WebKit::WebInputEvent::MouseMove;
  host_->ForwardMouseEvent(e);
}

void RenderWidgetHostViewViews::OnMouseEntered(const views::MouseEvent& event) {
  // Already generated synthetically by webkit.
}

void RenderWidgetHostViewViews::OnMouseExited(const views::MouseEvent& event) {
  // Already generated synthetically by webkit.
}

bool RenderWidgetHostViewViews::OnKeyPressed(const views::KeyEvent& event) {
  // TODO(suzhe): Support editor key bindings.
  if (!host_)
    return false;
  host_->ForwardKeyboardEvent(NativeWebKeyboardEventViews(event));
  return true;
}

bool RenderWidgetHostViewViews::OnKeyReleased(const views::KeyEvent& event) {
  if (!host_)
    return false;
  host_->ForwardKeyboardEvent(NativeWebKeyboardEventViews(event));
  return true;
}

bool RenderWidgetHostViewViews::OnMouseWheel(
    const views::MouseWheelEvent& event) {
  if (!host_)
    return false;

  WebMouseWheelEvent wmwe;
  InitializeWebMouseEventFromViewsEvent(event, GetMirroredPosition(), &wmwe);

  wmwe.type = WebKit::WebInputEvent::MouseWheel;
  wmwe.button = WebKit::WebMouseEvent::ButtonNone;

  // TODO(sadrul): How do we determine if it's a horizontal scroll?
  wmwe.deltaY = event.offset();
  wmwe.wheelTicksY = wmwe.deltaY > 0 ? 1 : -1;

  host_->ForwardWheelEvent(wmwe);
  return true;
}

ui::TextInputClient* RenderWidgetHostViewViews::GetTextInputClient() {
  return this;
}

bool RenderWidgetHostViewViews::GetTooltipText(const gfx::Point& p,
                                               string16* tooltip) const {
  if (tooltip_text_.length() == 0)
    return false;
  *tooltip = tooltip_text_;
  return true;
}

// ui::TextInputClient implementation -----------------------------------------
void RenderWidgetHostViewViews::SetCompositionText(
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

void RenderWidgetHostViewViews::ConfirmCompositionText() {
  if (host_ && has_composition_text_)
    host_->ImeConfirmComposition();
  has_composition_text_ = false;
}

void RenderWidgetHostViewViews::ClearCompositionText() {
  if (host_ && has_composition_text_)
    host_->ImeCancelComposition();
  has_composition_text_ = false;
}

void RenderWidgetHostViewViews::InsertText(const string16& text) {
  DCHECK(text_input_type_ != ui::TEXT_INPUT_TYPE_NONE);
  if (host_)
    host_->ImeConfirmComposition(text);
  has_composition_text_ = false;
}

void RenderWidgetHostViewViews::InsertChar(char16 ch, int flags) {
  if (host_) {
    NativeWebKeyboardEventViews::FromViewsEvent from_views_event;
    NativeWebKeyboardEventViews wke(ch, flags, base::Time::Now().ToDoubleT(),
                                    from_views_event);
    host_->ForwardKeyboardEvent(wke);
  }
}

ui::TextInputType RenderWidgetHostViewViews::GetTextInputType() const {
  return text_input_type_;
}

gfx::Rect RenderWidgetHostViewViews::GetCaretBounds() {
  return selection_start_rect_.Union(selection_end_rect_);
}

bool RenderWidgetHostViewViews::HasCompositionText() {
  return has_composition_text_;
}

bool RenderWidgetHostViewViews::GetTextRange(ui::Range* range) {
  range->set_start(selection_text_offset_);
  range->set_end(selection_text_offset_ + selection_text_.length());
  return true;
}

bool RenderWidgetHostViewViews::GetCompositionTextRange(ui::Range* range) {
  // TODO(suzhe): implement this method when fixing http://crbug.com/55130.
  NOTIMPLEMENTED();
  return false;
}

bool RenderWidgetHostViewViews::GetSelectionRange(ui::Range* range) {
  range->set_start(selection_range_.start());
  range->set_end(selection_range_.end());
  return true;
}

bool RenderWidgetHostViewViews::SetSelectionRange(const ui::Range& range) {
  // TODO(suzhe): implement this method when fixing http://crbug.com/55130.
  NOTIMPLEMENTED();
  return false;
}

bool RenderWidgetHostViewViews::DeleteRange(const ui::Range& range) {
  // TODO(suzhe): implement this method when fixing http://crbug.com/55130.
  NOTIMPLEMENTED();
  return false;
}

bool RenderWidgetHostViewViews::GetTextFromRange(
    const ui::Range& range,
    string16* text) {
  ui::Range selection_text_range(selection_text_offset_,
      selection_text_offset_ + selection_text_.length());

  if (!selection_text_range.Contains(range)) {
    text->clear();
    return false;
  }
  if (selection_text_range.EqualsIgnoringDirection(range)) {
    // Avoid calling substr which performance is low.
    *text = selection_text_;
  } else {
    *text = selection_text_.substr(
        range.GetMin() - selection_text_offset_,
        range.length());
  }
  return true;
}

void RenderWidgetHostViewViews::OnInputMethodChanged() {
  if (!host_)
    return;

  DCHECK(GetInputMethod());
  host_->SetInputMethodActive(GetInputMethod()->IsActive());

  // TODO(suzhe): implement the newly added “locale” property of HTML DOM
  // TextEvent.
}

bool RenderWidgetHostViewViews::ChangeTextDirectionAndLayoutAlignment(
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

void RenderWidgetHostViewViews::OnPaint(gfx::Canvas* canvas) {
  if (is_hidden_ || !host_)
    return;

  DCHECK(!host_->is_accelerated_compositing_active() ||
         get_use_acceleration_when_possible());

  // If we aren't using the views compositor, then
  // paint a "hole" in the canvas so that the render of the web page is on
  // top of whatever else has already been painted in the views hierarchy.
  // Later views might still get to paint on top.
  if (!get_use_acceleration_when_possible())
    canvas->FillRect(SK_ColorBLACK, GetLocalBounds(), SkXfermode::kClear_Mode);

  DCHECK(!about_to_validate_and_paint_);

  // TODO(anicolao): get the damage somehow
  // invalid_rect_ = damage_rect;
  invalid_rect_ = bounds();
  gfx::Point origin;
  ConvertPointToWidget(this, &origin);

  about_to_validate_and_paint_ = true;
  BackingStore* backing_store = host_->GetBackingStore(true);
  // Calling GetBackingStore maybe have changed |invalid_rect_|...
  about_to_validate_and_paint_ = false;

  gfx::Rect paint_rect = gfx::Rect(0, 0, kMaxWindowWidth, kMaxWindowHeight);
  paint_rect = paint_rect.Intersect(invalid_rect_);

  if (backing_store) {
#if defined(TOOLKIT_USES_GTK)
    // Only render the widget if it is attached to a window; there's a short
    // period where this object isn't attached to a window but hasn't been
    // Destroy()ed yet and it receives paint messages...
    if (IsReadyToPaint()) {
#endif
      if (!visually_deemphasized_) {
        // In the common case, use XCopyArea. We don't draw more than once, so
        // we don't need to double buffer.
        if (IsPopup()) {
          origin.SetPoint(origin.x() + paint_rect.x(),
                          origin.y() + paint_rect.y());
          paint_rect.SetRect(0, 0, paint_rect.width(), paint_rect.height());
        }
        static_cast<BackingStoreSkia*>(backing_store)->SkiaShowRect(
            gfx::Point(paint_rect.x(), paint_rect.y()), canvas);
      } else {
        // TODO(sad)
        NOTIMPLEMENTED();
      }
#if defined(TOOLKIT_USES_GTK)
    }
#endif
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
    if (whiteout_start_time_.is_null())
      whiteout_start_time_ = base::TimeTicks::Now();

    if (get_use_acceleration_when_possible())
      canvas->FillRect(SK_ColorWHITE, GetLocalBounds());
  }
}

void RenderWidgetHostViewViews::Focus() {
  RequestFocus();
}

void RenderWidgetHostViewViews::Blur() {
  // TODO(estade): We should be clearing native focus as well, but I know of no
  // way to do that without focusing another widget.
  if (host_) {
    host_->SetActive(false);
    host_->Blur();
  }
}

void RenderWidgetHostViewViews::OnFocus() {
  if (!host_)
    return;

  DCHECK(GetInputMethod());
  View::OnFocus();
  ShowCurrentCursor();
  host_->GotFocus();
  host_->SetActive(true);
  host_->SetInputMethodActive(GetInputMethod()->IsActive());

  UpdateTouchSelectionController();
}

void RenderWidgetHostViewViews::OnBlur() {
  if (!host_)
    return;
  View::OnBlur();
  // If we are showing a context menu, maintain the illusion that webkit has
  // focus.
  if (!is_showing_context_menu_ && !is_hidden_)
    host_->Blur();
  host_->SetInputMethodActive(false);

  if (touch_selection_controller_.get())
    touch_selection_controller_->ClientViewLostFocus();
}

bool RenderWidgetHostViewViews::NeedsInputGrab() {
  return popup_type_ == WebKit::WebPopupTypeSelect;
}

bool RenderWidgetHostViewViews::IsPopup() {
  return popup_type_ != WebKit::WebPopupTypeNone;
}

WebKit::WebMouseEvent RenderWidgetHostViewViews::WebMouseEventFromViewsEvent(
    const views::MouseEvent& event) {
  WebKit::WebMouseEvent wmevent;
  InitializeWebMouseEventFromViewsEvent(event, GetMirroredPosition(), &wmevent);

  // Setting |wmevent.button| is not necessary for -move events, but it is
  // necessary for -clicks and -drags.
  if (event.IsMiddleMouseButton()) {
    wmevent.modifiers |= WebKit::WebInputEvent::MiddleButtonDown;
    wmevent.button = WebKit::WebMouseEvent::ButtonMiddle;
  }
  if (event.IsRightMouseButton()) {
    wmevent.modifiers |= WebKit::WebInputEvent::RightButtonDown;
    wmevent.button = WebKit::WebMouseEvent::ButtonRight;
  }
  if (event.IsLeftMouseButton()) {
    wmevent.modifiers |= WebKit::WebInputEvent::LeftButtonDown;
    wmevent.button = WebKit::WebMouseEvent::ButtonLeft;
  }

  return wmevent;
}

void RenderWidgetHostViewViews::FinishImeCompositionSession() {
  if (!has_composition_text_)
    return;
  if (host_)
    host_->ImeConfirmComposition();
  DCHECK(GetInputMethod());
  GetInputMethod()->CancelComposition(this);
  has_composition_text_ = false;
}

void RenderWidgetHostViewViews::UpdateTouchSelectionController() {
  gfx::Point start = selection_start_rect_.origin();
  start.Offset(0, selection_start_rect_.height());
  gfx::Point end = selection_end_rect_.origin();
  end.Offset(0, selection_end_rect_.height());

  if (touch_selection_controller_.get())
    touch_selection_controller_->SelectionChanged(start, end);
}

#if !defined(OS_WIN)
void RenderWidgetHostViewViews::UpdateCursor(const WebCursor& cursor) {
  // Optimize the common case, where the cursor hasn't changed.
  // However, we can switch between different pixmaps, so only on the
  // non-pixmap branch.
#if defined(TOOLKIT_USES_GTK)
  if (current_cursor_.GetCursorType() != GDK_CURSOR_IS_PIXMAP &&
      current_cursor_.GetCursorType() == cursor.GetCursorType()) {
    return;
  }
#endif
  current_cursor_ = cursor;
  ShowCurrentCursor();
}

void RenderWidgetHostViewViews::ShowCurrentCursor() {
#if !defined(USE_AURA)
  // The widget may not have a window. If that's the case, abort mission. This
  // is the same issue as that explained above in Paint().
  if (!IsReadyToPaint())
    return;
#endif

  native_cursor_ = current_cursor_.GetNativeCursor();
}

#if defined(TOOLKIT_USES_GTK)
bool RenderWidgetHostViewViews::IsReadyToPaint() {
  views::Widget* top = NULL;

  // TODO(oshima): move this functionality to Widget.
  if (views::ViewsDelegate::views_delegate &&
      views::ViewsDelegate::views_delegate->GetDefaultParentView()) {
    top = views::ViewsDelegate::views_delegate->GetDefaultParentView()->
        GetWidget();
  } else {
    // Use GetTopLevelWidget()'s native view to get platform
    // native widget. This is a workaround to get window's NativeWidgetGtk
    // under both views desktop (where toplevel is NativeWidgetViews,
    // whose GetNativeView returns gtk widget of the native window)
    // and non views desktop (where toplevel is NativeWidgetGtk).
    top = GetWidget() ?
        views::Widget::GetWidgetForNativeView(
            GetWidget()->GetTopLevelWidget()->GetNativeView()) :
        NULL;
  }

  return top ?
      !!(static_cast<const views::NativeWidgetGtk*>(top->native_widget())->
         window_contents()->window) : false;
}
#endif

#endif  // !OS_WIN

#if defined(TOOLKIT_USES_GTK)
void RenderWidgetHostViewViews::CreatePluginContainer(
    gfx::PluginWindowHandle id) {
  // TODO(anicolao): plugin_container_manager_.CreatePluginContainer(id);
}

void RenderWidgetHostViewViews::DestroyPluginContainer(
    gfx::PluginWindowHandle id) {
  // TODO(anicolao): plugin_container_manager_.DestroyPluginContainer(id);
}

#endif  // TOOLKIT_USES_GTK

#if defined(OS_POSIX) || defined(USE_AURA)
void RenderWidgetHostViewViews::GetDefaultScreenInfo(
    WebKit::WebScreenInfo* results) {
  NOTIMPLEMENTED();
}

void RenderWidgetHostViewViews::GetScreenInfo(WebKit::WebScreenInfo* results) {
#if !defined(USE_AURA)
  views::Widget* widget = GetWidget() ? GetWidget()->GetTopLevelWidget() : NULL;
  if (widget && widget->GetNativeView())
    content::GetScreenInfoFromNativeWindow(widget->GetNativeView()->window,
                                           results);
  else
    RenderWidgetHostView::GetDefaultScreenInfo(results);
#else
  RenderWidgetHostView::GetDefaultScreenInfo(results);
#endif
}

gfx::Rect RenderWidgetHostViewViews::GetRootWindowBounds() {
  views::Widget* widget = GetWidget() ? GetWidget()->GetTopLevelWidget() : NULL;
  return widget ? widget->GetWindowScreenBounds() : gfx::Rect();
}
#endif

#if !defined(OS_WIN) && !defined(UI_COMPOSITOR_IMAGE_TRANSPORT)
gfx::PluginWindowHandle RenderWidgetHostViewViews::GetCompositingSurface() {
  // TODO(oshima): The original implementation was broken as
  // GtkNativeViewManager doesn't know about NativeWidgetGtk. Figure
  // out if this makes sense without compositor. If it does, then find
  // out the right way to handle.
  NOTIMPLEMENTED();
  return gfx::kNullPluginWindow;
}
#endif

#if defined(UI_COMPOSITOR_IMAGE_TRANSPORT)
gfx::PluginWindowHandle RenderWidgetHostViewViews::GetCompositingSurface() {
  // On TOUCH_UI builds, the GPU process renders to an offscreen surface
  // (created by the GPU process), which is later displayed by the browser.
  // As the GPU process creates this surface, we can return any non-zero value.
  return 1;
}

void RenderWidgetHostViewViews::AcceleratedSurfaceNew(
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

void RenderWidgetHostViewViews::AcceleratedSurfaceRelease(uint64 surface_id) {
  accelerated_surface_containers_.erase(surface_id);
}

void RenderWidgetHostViewViews::AcceleratedSurfaceBuffersSwapped(
    const GpuHostMsg_AcceleratedSurfaceBuffersSwapped_Params& params,
    int gpu_host_id) {
  SetExternalTexture(
      accelerated_surface_containers_[params.surface_id]->GetTexture());
  glFlush();

  if (!GetWidget() || !GetWidget()->GetCompositor()) {
    // We have no compositor, so we have no way to display the surface
    // Must still send the ACK
    AcknowledgeSwapBuffers(params.route_id, gpu_host_id);
  } else {
    // Add sending an ACK to the list of things to do OnCompositingEnded
    on_compositing_ended_callbacks_.push_back(
        base::Bind(AcknowledgeSwapBuffers, params.route_id, gpu_host_id));
    ui::Compositor *compositor = GetWidget()->GetCompositor();
    if (!compositor->HasObserver(this))
      compositor->AddObserver(this);
  }
}

void RenderWidgetHostViewViews::OnCompositingEnded(ui::Compositor* compositor) {
  for (std::vector< base::Callback<void(void)> >::const_iterator
      it = on_compositing_ended_callbacks_.begin();
      it != on_compositing_ended_callbacks_.end(); ++it) {
    it->Run();
  }
  on_compositing_ended_callbacks_.clear();
  compositor->RemoveObserver(this);
}

#else

void RenderWidgetHostViewViews::AcceleratedSurfaceBuffersSwapped(
    const GpuHostMsg_AcceleratedSurfaceBuffersSwapped_Params& params,
    int gpu_host_id) {
  NOTREACHED();
}

#endif
