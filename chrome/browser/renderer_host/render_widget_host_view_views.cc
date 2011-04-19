// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/render_widget_host_view_views.h"

#include <algorithm>
#include <string>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/string_number_conversions.h"
#include "base/task.h"
#include "base/time.h"
#include "chrome/common/render_messages.h"
#include "content/browser/renderer_host/backing_store_skia.h"
#include "content/browser/renderer_host/backing_store_x.h"
#include "content/browser/renderer_host/render_widget_host.h"
#include "content/common/native_web_keyboard_event.h"
#include "content/common/result_codes.h"
#include "content/common/view_messages.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/gtk/WebInputEventFactory.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebInputEvent.h"
#include "ui/base/keycodes/keyboard_code_conversion_gtk.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/x/x11_util.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/canvas_skia.h"
#include "ui/gfx/gtk_native_view_id_manager.h"
#include "views/events/event.h"
#include "views/ime/input_method.h"
#include "views/widget/widget.h"
#include "views/widget/widget_gtk.h"

static const int kMaxWindowWidth = 4000;
static const int kMaxWindowHeight = 4000;
static const char kRenderWidgetHostViewKey[] = "__RENDER_WIDGET_HOST_VIEW__";
static const char kBackingStoreSkiaSwitch[] = "use-backing-store-skia";

// Copied from third_party/WebKit/Source/WebCore/page/EventHandler.cpp
//
// Match key code of composition keydown event on windows.
// IE sends VK_PROCESSKEY which has value 229;
//
// Please refer to following documents for detals:
// - Virtual-Key Codes
//   http://msdn.microsoft.com/en-us/library/ms645540(VS.85).aspx
// - How the IME System Works
//   http://msdn.microsoft.com/en-us/library/cc194848.aspx
// - ImmGetVirtualKey Function
//   http://msdn.microsoft.com/en-us/library/dd318570(VS.85).aspx
static const int kCompositionEventKeyCode = 229;

using WebKit::WebInputEventFactory;
using WebKit::WebMouseWheelEvent;
using WebKit::WebTouchEvent;

const char RenderWidgetHostViewViews::kViewClassName[] =
    "browser/renderer_host/RenderWidgetHostViewViews";

namespace {

bool UsingBackingStoreSkia() {
  static bool decided = false;
  static bool use_skia = false;
  if (!decided) {
    CommandLine* cmdline = CommandLine::ForCurrentProcess();
    use_skia = (cmdline && cmdline->HasSwitch(kBackingStoreSkiaSwitch));
    decided = true;
  }

  return use_skia;
}

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

WebKit::WebTouchPoint::State TouchPointStateFromEvent(
    const views::TouchEvent* event) {
  switch (event->type()) {
    case ui::ET_TOUCH_PRESSED:
      return WebKit::WebTouchPoint::StatePressed;
    case ui::ET_TOUCH_RELEASED:
      return WebKit::WebTouchPoint::StateReleased;
    case ui::ET_TOUCH_MOVED:
      return WebKit::WebTouchPoint::StateMoved;
    case ui::ET_TOUCH_CANCELLED:
      return WebKit::WebTouchPoint::StateCancelled;
    default:
      return WebKit::WebTouchPoint::StateUndefined;
  }
}

WebKit::WebInputEvent::Type TouchEventTypeFromEvent(
    const views::TouchEvent* event) {
  switch (event->type()) {
    case ui::ET_TOUCH_PRESSED:
      return WebKit::WebInputEvent::TouchStart;
    case ui::ET_TOUCH_RELEASED:
      return WebKit::WebInputEvent::TouchEnd;
    case ui::ET_TOUCH_MOVED:
      return WebKit::WebInputEvent::TouchMove;
    case ui::ET_TOUCH_CANCELLED:
      return WebKit::WebInputEvent::TouchCancel;
    default:
      return WebKit::WebInputEvent::Undefined;
  }
}

void UpdateTouchPointPosition(const views::TouchEvent* event,
                              const gfx::Point& origin,
                              WebKit::WebTouchPoint* tpoint) {
  tpoint->position.x = event->x();
  tpoint->position.y = event->y();

  tpoint->screenPosition.x = tpoint->position.x + origin.x();
  tpoint->screenPosition.y = tpoint->position.y + origin.y();
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

}  // namespace

// static
RenderWidgetHostView* RenderWidgetHostView::CreateViewForWidget(
    RenderWidgetHost* widget) {
  return new RenderWidgetHostViewViews(widget);
}

RenderWidgetHostViewViews::RenderWidgetHostViewViews(RenderWidgetHost* host)
    : host_(host),
      about_to_validate_and_paint_(false),
      is_hidden_(false),
      is_loading_(false),
      native_cursor_(NULL),
      is_showing_context_menu_(false),
      visually_deemphasized_(false),
      touch_event_(),
      text_input_type_(ui::TEXT_INPUT_TYPE_NONE),
      has_composition_text_(false) {
  SetFocusable(true);
  host_->set_view(this);
}

RenderWidgetHostViewViews::~RenderWidgetHostViewViews() {
}

void RenderWidgetHostViewViews::InitAsChild() {
  Show();
}

void RenderWidgetHostViewViews::InitAsPopup(
    RenderWidgetHostView* parent_host_view,
    const gfx::Rect& pos) {
  // TODO(anicolao): figure out cases where popups occur and implement
  NOTIMPLEMENTED();
}

void RenderWidgetHostViewViews::InitAsFullscreen() {
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
  NOTIMPLEMENTED();
}

gfx::NativeView RenderWidgetHostViewViews::GetNativeView() {
  if (GetWidget())
    return GetWidget()->GetNativeView();
  return NULL;
}

void RenderWidgetHostViewViews::MovePluginWindows(
    const std::vector<webkit::npapi::WebPluginGeometry>& moves) {
  // TODO(anicolao): NIY
  // NOTIMPLEMENTED();
}

bool RenderWidgetHostViewViews::HasFocus() {
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
  return bounds();
}

void RenderWidgetHostViewViews::UpdateCursor(const WebCursor& cursor) {
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

void RenderWidgetHostViewViews::SetIsLoading(bool is_loading) {
  is_loading_ = is_loading;
  // Only call ShowCurrentCursor() when it will actually change the cursor.
  if (current_cursor_.GetCursorType() == GDK_LAST_CURSOR)
    ShowCurrentCursor();
}

void RenderWidgetHostViewViews::ImeUpdateTextInputState(
    WebKit::WebTextInputType type,
    const gfx::Rect& caret_rect) {
  DCHECK(GetInputMethod());
  ui::TextInputType new_type = static_cast<ui::TextInputType>(type);
  if (text_input_type_ != new_type) {
    text_input_type_ = new_type;
    GetInputMethod()->OnTextInputTypeChanged(this);
  }
  if (caret_bounds_ != caret_rect) {
    caret_bounds_ = caret_rect;
    GetInputMethod()->OnCaretBoundsChanged(this);
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

  if (parent())
    parent()->RemoveChildView(this);
  MessageLoop::current()->DeleteSoon(FROM_HERE, this);
}

void RenderWidgetHostViewViews::SetTooltipText(const std::wstring& tip) {
  // TODO(anicolao): decide if we want tooltips for touch (none specified
  // right now/might want a press-and-hold display)
  // NOTIMPLEMENTED(); ... too annoying, it triggers for every mousemove
}

void RenderWidgetHostViewViews::SelectionChanged(const std::string& text) {
  // TODO(anicolao): deal with the clipboard without GTK
  NOTIMPLEMENTED();
}

void RenderWidgetHostViewViews::ShowingContextMenu(bool showing) {
  is_showing_context_menu_ = showing;
}

BackingStore* RenderWidgetHostViewViews::AllocBackingStore(
    const gfx::Size& size) {
  gfx::NativeView nview = GetInnerNativeView();
  if (!nview)
    return NULL;

  if (UsingBackingStoreSkia()) {
    return new BackingStoreSkia(host_, size);
  } else {
    return new BackingStoreX(host_, size,
                             ui::GetVisualFromGtkWidget(nview),
                             gtk_widget_get_visual(nview)->depth);
  }
}

void RenderWidgetHostViewViews::SetBackground(const SkBitmap& background) {
  RenderWidgetHostView::SetBackground(background);
  if (host_)
    host_->Send(new ViewMsg_SetBackground(host_->routing_id(), background));
}

void RenderWidgetHostViewViews::CreatePluginContainer(
    gfx::PluginWindowHandle id) {
  // TODO(anicolao): plugin_container_manager_.CreatePluginContainer(id);
}

void RenderWidgetHostViewViews::DestroyPluginContainer(
    gfx::PluginWindowHandle id) {
  // TODO(anicolao): plugin_container_manager_.DestroyPluginContainer(id);
}

void RenderWidgetHostViewViews::SetVisuallyDeemphasized(
    const SkColor* color, bool animate) {
  // TODO(anicolao)
}

bool RenderWidgetHostViewViews::ContainsNativeView(
    gfx::NativeView native_view) const {
  // TODO(port)
  NOTREACHED() <<
    "RenderWidgetHostViewViews::ContainsNativeView not implemented.";
  return false;
}

void RenderWidgetHostViewViews::AcceleratedCompositingActivated(
    bool activated) {
  // TODO(anicolao): figure out if we need something here
  if (activated)
    NOTIMPLEMENTED();
}

gfx::PluginWindowHandle RenderWidgetHostViewViews::GetCompositingSurface() {
  GtkNativeViewManager* manager = GtkNativeViewManager::GetInstance();
  gfx::PluginWindowHandle surface = gfx::kNullPluginWindow;
  gfx::NativeViewId view_id = gfx::IdFromNativeView(GetInnerNativeView());

  if (!manager->GetXIDForId(&surface, view_id)) {
    DLOG(ERROR) << "Can't find XID for view id " << view_id;
  }
  return surface;
}

gfx::NativeView RenderWidgetHostViewViews::GetInnerNativeView() const {
  // TODO(sad): Ideally this function should be equivalent to GetNativeView, and
  // WidgetGtk-specific function call should not be necessary.
  const views::WidgetGtk* widget =
      static_cast<const views::WidgetGtk*>(GetWidget());
  return widget ? widget->window_contents() : NULL;
}

std::string RenderWidgetHostViewViews::GetClassName() const {
  return kViewClassName;
}

gfx::NativeCursor RenderWidgetHostViewViews::GetCursorForPoint(
    ui::EventType type, const gfx::Point& point) {
  return native_cursor_;
}

bool RenderWidgetHostViewViews::OnMousePressed(const views::MouseEvent& event) {
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

views::View::TouchStatus RenderWidgetHostViewViews::OnTouchEvent(
    const views::TouchEvent& event) {
  if (!host_)
    return TOUCH_STATUS_UNKNOWN;

  // Update the list of touch points first.
  WebKit::WebTouchPoint* point = NULL;
  TouchStatus status = TOUCH_STATUS_UNKNOWN;

  switch (event.type()) {
    case ui::ET_TOUCH_PRESSED:
      // Add a new touch point.
      if (touch_event_.touchPointsLength <
          WebTouchEvent::touchPointsLengthCap) {
        point = &touch_event_.touchPoints[touch_event_.touchPointsLength++];
        point->id = event.identity();

        if (touch_event_.touchPointsLength == 1) {
          // A new touch sequence has started.
          status = TOUCH_STATUS_START;

          // We also want the focus.
          RequestFocus();

          // Confirm existing composition text on touch press events, to make
          // sure the input caret won't be moved with an ongoing composition
          // text.
          FinishImeCompositionSession();
        }
      }
      break;
    case ui::ET_TOUCH_RELEASED:
    case ui::ET_TOUCH_CANCELLED:
    case ui::ET_TOUCH_MOVED: {
      // The touch point should have been added to the event from an earlier
      // _PRESSED event. So find that.
      // At the moment, only a maximum of 4 touch-points are allowed. So a
      // simple loop should be sufficient.
      for (int i = 0; i < touch_event_.touchPointsLength; ++i) {
        point = touch_event_.touchPoints + i;
        if (point->id == event.identity()) {
          break;
        }
        point = NULL;
      }
      break;
    }
    default:
      DLOG(WARNING) << "Unknown touch event " << event.type();
      break;
  }

  if (!point)
    return TOUCH_STATUS_UNKNOWN;

  if (status != TOUCH_STATUS_START)
    status = TOUCH_STATUS_CONTINUE;

  // Update the location and state of the point.
  point->state = TouchPointStateFromEvent(&event);
  if (point->state == WebKit::WebTouchPoint::StateMoved) {
    // It is possible for badly written touch drivers to emit Move events even
    // when the touch location hasn't changed. In such cases, consume the event
    // and pretend nothing happened.
    if (point->position.x == event.x() && point->position.y == event.y()) {
      return status;
    }
  }
  UpdateTouchPointPosition(&event, GetMirroredPosition(), point);

  // Mark the rest of the points as stationary.
  for (int i = 0; i < touch_event_.touchPointsLength; ++i) {
    WebKit::WebTouchPoint* iter = touch_event_.touchPoints + i;
    if (iter != point) {
      iter->state = WebKit::WebTouchPoint::StateStationary;
    }
  }

  // Update the type of the touch event.
  touch_event_.type = TouchEventTypeFromEvent(&event);
  touch_event_.timeStampSeconds = base::Time::Now().ToDoubleT();

  // The event and all the touches have been updated. Dispatch.
  host_->ForwardTouchEvent(touch_event_);

  // If the touch was released, then remove it from the list of touch points.
  if (event.type() == ui::ET_TOUCH_RELEASED) {
    --touch_event_.touchPointsLength;
    for (int i = point - touch_event_.touchPoints;
         i < touch_event_.touchPointsLength;
         ++i) {
      touch_event_.touchPoints[i] = touch_event_.touchPoints[i + 1];
    }
    if (touch_event_.touchPointsLength == 0)
      status = TOUCH_STATUS_END;
  } else if (event.type() == ui::ET_TOUCH_CANCELLED) {
    status = TOUCH_STATUS_CANCEL;
  }

  return status;
}

bool RenderWidgetHostViewViews::OnKeyPressed(const views::KeyEvent& event) {
  // TODO(suzhe): Support editor key bindings.
  if (!host_)
    return false;
  host_->ForwardKeyboardEvent(NativeWebKeyboardEvent(event));
  return true;
}

bool RenderWidgetHostViewViews::OnKeyReleased(const views::KeyEvent& event) {
  if (!host_)
    return false;
  host_->ForwardKeyboardEvent(NativeWebKeyboardEvent(event));
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

views::TextInputClient* RenderWidgetHostViewViews::GetTextInputClient() {
  return this;
}

// TextInputClient implementation ---------------------------------------------
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
    NativeWebKeyboardEvent::FromViewsEvent from_views_event;
    NativeWebKeyboardEvent wke(ch, flags, base::Time::Now().ToDoubleT(),
                               from_views_event);
    host_->ForwardKeyboardEvent(wke);
  }
}

ui::TextInputType RenderWidgetHostViewViews::GetTextInputType() {
  return text_input_type_;
}

gfx::Rect RenderWidgetHostViewViews::GetCaretBounds() {
  return caret_bounds_;
}

bool RenderWidgetHostViewViews::HasCompositionText() {
  return has_composition_text_;
}

bool RenderWidgetHostViewViews::GetTextRange(ui::Range* range) {
  // TODO(suzhe): implement this method when fixing http://crbug.com/55130.
  NOTIMPLEMENTED();
  return false;
}

bool RenderWidgetHostViewViews::GetCompositionTextRange(ui::Range* range) {
  // TODO(suzhe): implement this method when fixing http://crbug.com/55130.
  NOTIMPLEMENTED();
  return false;
}

bool RenderWidgetHostViewViews::GetSelectionRange(ui::Range* range) {
  // TODO(suzhe): implement this method when fixing http://crbug.com/55130.
  NOTIMPLEMENTED();
  return false;
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
    const base::Callback<void(const string16&)>& callback) {
  // TODO(suzhe): implement this method when fixing http://crbug.com/55130.
  NOTIMPLEMENTED();
  return false;
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

views::View* RenderWidgetHostViewViews::GetOwnerViewOfTextInputClient() {
  return this;
}

void RenderWidgetHostViewViews::OnPaint(gfx::Canvas* canvas) {
  if (is_hidden_ || !host_)
    return;

  // Paint a "hole" in the canvas so that the render of the web page is on
  // top of whatever else has already been painted in the views hierarchy.
  // Later views might still get to paint on top.
  canvas->FillRectInt(SK_ColorBLACK, 0, 0,
                      bounds().width(), bounds().height(),
                      SkXfermode::kClear_Mode);

  // Don't do any painting if the GPU process is rendering directly
  // into the View.
  if (host_->is_accelerated_compositing_active())
    return;

  GdkWindow* window = GetInnerNativeView()->window;
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
    // Only render the widget if it is attached to a window; there's a short
    // period where this object isn't attached to a window but hasn't been
    // Destroy()ed yet and it receives paint messages...
    if (window) {
      if (!visually_deemphasized_) {
        // In the common case, use XCopyArea. We don't draw more than once, so
        // we don't need to double buffer.

        if (UsingBackingStoreSkia()) {
          static_cast<BackingStoreSkia*>(backing_store)->SkiaShowRect(
              gfx::Point(paint_rect.x(), paint_rect.y()), canvas);
        } else {
          static_cast<BackingStoreX*>(backing_store)->XShowRect(origin,
              paint_rect, ui::GetX11WindowFromGdkWindow(window));
        }
      } else if (!UsingBackingStoreSkia()) {
        // If the grey blend is showing, we make two drawing calls. Use double
        // buffering to prevent flicker. Use CairoShowRect because XShowRect
        // shortcuts GDK's double buffering.
        GdkRectangle rect = { paint_rect.x(), paint_rect.y(),
                              paint_rect.width(), paint_rect.height() };
        gdk_window_begin_paint_rect(window, &rect);

        static_cast<BackingStoreX*>(backing_store)->CairoShowRect(
            paint_rect, GDK_DRAWABLE(window));

        cairo_t* cr = gdk_cairo_create(window);
        gdk_cairo_rectangle(cr, &rect);
        cairo_set_source_rgba(cr, 0, 0, 0, 0.7);
        cairo_fill(cr);
        cairo_destroy(cr);

        gdk_window_end_paint(window);
      } else {
        // TODO(sad)
        NOTIMPLEMENTED();
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
    if (whiteout_start_time_.is_null())
      whiteout_start_time_ = base::TimeTicks::Now();
  }
}

void RenderWidgetHostViewViews::Focus() {
  RequestFocus();
}

void RenderWidgetHostViewViews::Blur() {
  // TODO(estade): We should be clearing native focus as well, but I know of no
  // way to do that without focusing another widget.
  if (host_)
    host_->Blur();
}

void RenderWidgetHostViewViews::OnFocus() {
  if (!host_)
    return;

  DCHECK(GetInputMethod());
  View::OnFocus();
  ShowCurrentCursor();
  host_->GotFocus();
  host_->SetInputMethodActive(GetInputMethod()->IsActive());
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
}

bool RenderWidgetHostViewViews::NeedsInputGrab() {
  return popup_type_ == WebKit::WebPopupTypeSelect;
}

bool RenderWidgetHostViewViews::IsPopup() {
  return popup_type_ != WebKit::WebPopupTypeNone;
}

void RenderWidgetHostViewViews::ShowCurrentCursor() {
  // The widget may not have a window. If that's the case, abort mission. This
  // is the same issue as that explained above in Paint().
  if (!GetInnerNativeView() || !GetInnerNativeView()->window)
    return;

  native_cursor_ = current_cursor_.GetNativeCursor();
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

// static
RenderWidgetHostView*
    RenderWidgetHostView::GetRenderWidgetHostViewFromNativeView(
        gfx::NativeView widget) {
  gpointer user_data = g_object_get_data(G_OBJECT(widget),
                                         kRenderWidgetHostViewKey);
  return reinterpret_cast<RenderWidgetHostView*>(user_data);
}
