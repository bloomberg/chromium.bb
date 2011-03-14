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
#include "chrome/common/native_web_keyboard_event.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/result_codes.h"
#include "content/browser/renderer_host/backing_store_skia.h"
#include "content/browser/renderer_host/backing_store_x.h"
#include "content/browser/renderer_host/render_widget_host.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/gtk/WebInputEventFactory.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebInputEvent.h"
#include "ui/base/keycodes/keyboard_code_conversion_gtk.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/x/x11_util.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/canvas_skia.h"
#include "ui/gfx/gtk_native_view_id_manager.h"
#include "views/events/event.h"
#include "views/ime/ime_context.h"
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

class IMEContextHandler : public views::CommitTextListener,
                          public views::CompositionListener,
                          public views::ForwardKeyEventListener {
 public:
  explicit IMEContextHandler(
      RenderWidgetHostViewViews* host_view)
    : host_view_(host_view),
      is_enabled_(false),
      is_focused_(false),
      ime_context_(views::IMEContext::Create(host_view_)) {
    ime_context_->set_commit_text_listener(this);
    ime_context_->set_composition_listener(this);
    ime_context_->set_forward_key_event_listener(this);
  }

  // IMEContext Listeners implementation
  virtual void OnCommitText(views::IMEContext* sender,
                            const string16& text) {
    DCHECK(ime_context_ == sender);

    RenderWidgetHost* host = host_view_->GetRenderWidgetHost();
    if (host) {
      SendFakeCompositionWebKeyboardEvent(WebKit::WebInputEvent::RawKeyDown);
      host->ImeConfirmComposition(text);
      SendFakeCompositionWebKeyboardEvent(WebKit::WebInputEvent::KeyUp);
    }
  }

  virtual void OnStartComposition(views::IMEContext* sender) {
    DCHECK(ime_context_ == sender);
  }

  virtual void OnEndComposition(views::IMEContext* sender) {
    DCHECK(ime_context_ == sender);

    RenderWidgetHost* host = host_view_->GetRenderWidgetHost();
    if (host)
      host->ImeCancelComposition();
  }

  virtual void OnSetComposition(views::IMEContext* sender,
      const string16& text,
      const views::CompositionAttributeList& attributes,
      uint32 cursor_pos) {
    DCHECK(ime_context_ == sender);

    RenderWidgetHost* host = host_view_->GetRenderWidgetHost();
    if (host) {
      SendFakeCompositionWebKeyboardEvent(WebKit::WebInputEvent::RawKeyDown);

      // Cast CompositonAttribute to WebKit::WebCompositionUnderline directly,
      // becasue CompositionAttribute is duplicated from
      // WebKit::WebCompositionUnderline.
      const std::vector<WebKit::WebCompositionUnderline>& underlines =
          reinterpret_cast<const std::vector<WebKit::WebCompositionUnderline>&>(
              attributes);
      host->ImeSetComposition(text, underlines, cursor_pos, cursor_pos);
      SendFakeCompositionWebKeyboardEvent(WebKit::WebInputEvent::KeyUp);
    }
  }

  virtual void OnForwardKeyEvent(views::IMEContext* sender,
                                 const views::KeyEvent& event) {
    DCHECK(ime_context_ == sender);

    host_view_->ForwardKeyEvent(event);
  }

  bool FilterKeyEvent(const views::KeyEvent& event) {
    return is_enabled_ && ime_context_->FilterKeyEvent(event);
  }

  void Focus() {
    if (!is_focused_) {
      ime_context_->Focus();
      is_focused_ = true;
    }

    // Enables RenderWidget's IME related events, so that we can be notified
    // when WebKit wants to enable or disable IME.
    RenderWidgetHost* host = host_view_->GetRenderWidgetHost();
    if (host)
      host->SetInputMethodActive(true);
  }

  void Blur() {
    if (is_focused_) {
      ime_context_->Blur();
      is_focused_ = false;
    }

    // Disable RenderWidget's IME related events to save bandwidth.
    RenderWidgetHost* host = host_view_->GetRenderWidgetHost();
    if (host)
      host->SetInputMethodActive(false);
  }

  void ImeUpdateTextInputState(WebKit::WebTextInputType type,
                               const gfx::Rect& caret_rect) {
    bool enable =
        (type != WebKit::WebTextInputTypeNone) &&
        (type != WebKit::WebTextInputTypePassword);

    if (is_enabled_ != enable) {
      is_enabled_ = enable;
      if (is_focused_) {
        if (is_enabled_)
          ime_context_->Focus();
        else
          ime_context_->Blur();
      }
    }

    if (is_enabled_) {
      gfx::Point p(caret_rect.origin());
      views::View::ConvertPointToScreen(host_view_, &p);

      ime_context_->SetCursorLocation(gfx::Rect(p, caret_rect.size()));
    }
  }

  void Reset() {
    ime_context_->Reset();
  }

 private:
  void SendFakeCompositionWebKeyboardEvent(WebKit::WebInputEvent::Type type) {
    NativeWebKeyboardEvent fake_event;
    fake_event.windowsKeyCode = kCompositionEventKeyCode;
    fake_event.skip_in_browser = true;
    fake_event.type = type;
    host_view_->ForwardWebKeyboardEvent(fake_event);
  }

 private:
  RenderWidgetHostViewViews* host_view_;
  bool is_enabled_;
  bool is_focused_;
  scoped_ptr<views::IMEContext> ime_context_;

  DISALLOW_COPY_AND_ASSIGN(IMEContextHandler);
};

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
      touch_event_() {
  SetFocusable(true);
  host_->set_view(this);
}

RenderWidgetHostViewViews::~RenderWidgetHostViewViews() {
}

void RenderWidgetHostViewViews::InitAsChild() {
  Show();
  ime_context_.reset(new IMEContextHandler(this));
}

RenderWidgetHost* RenderWidgetHostViewViews::GetRenderWidgetHost() const {
  return host_;
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

void RenderWidgetHostViewViews::DidBecomeSelected() {
  if (!is_hidden_)
    return;

  if (tab_switch_paint_time_.is_null())
    tab_switch_paint_time_ = base::TimeTicks::Now();
  is_hidden_ = false;
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
  GetRenderWidgetHost()->WasHidden();
}

void RenderWidgetHostViewViews::SetSize(const gfx::Size& size) {
  // This is called when webkit has sent us a Move message.
  int width = std::min(size.width(), kMaxWindowWidth);
  int height = std::min(size.height(), kMaxWindowHeight);
  if (requested_size_.width() != width ||
      requested_size_.height() != height) {
    requested_size_ = gfx::Size(width, height);
    SetBounds(x(), y(), width, height);
    host_->WasResized();
  }
}

void RenderWidgetHostViewViews::MovePluginWindows(
    const std::vector<webkit::npapi::WebPluginGeometry>& moves) {
  // TODO(anicolao): NIY
  // NOTIMPLEMENTED();
}

void RenderWidgetHostViewViews::Focus() {
  RequestFocus();
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

void RenderWidgetHostViewViews::Blur() {
  // TODO(estade): We should be clearing native focus as well, but I know of no
  // way to do that without focusing another widget.
  host_->Blur();
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
  ime_context_->ImeUpdateTextInputState(type, caret_rect);
}

void RenderWidgetHostViewViews::ImeCancelComposition() {
  ime_context_->Reset();
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
  GetRenderWidgetHost()->ViewDestroyed();
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

bool RenderWidgetHostViewViews::NeedsInputGrab() {
  return popup_type_ == WebKit::WebPopupTypeSelect;
}

bool RenderWidgetHostViewViews::IsPopup() {
  return popup_type_ != WebKit::WebPopupTypeNone;
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

gfx::NativeView RenderWidgetHostViewViews::GetInnerNativeView() const {
  // TODO(sad): Ideally this function should be equivalent to GetNativeView, and
  // WidgetGtk-specific function call should not be necessary.
  const views::WidgetGtk* widget =
      static_cast<const views::WidgetGtk*>(GetWidget());
  return widget ? widget->window_contents() : NULL;
}

gfx::NativeView RenderWidgetHostViewViews::GetNativeView() {
  if (GetWidget())
    return GetWidget()->GetNativeView();
  return NULL;
}

void RenderWidgetHostViewViews::SetBackground(const SkBitmap& background) {
  RenderWidgetHostView::SetBackground(background);
  host_->Send(new ViewMsg_SetBackground(host_->routing_id(), background));
}

void RenderWidgetHostViewViews::OnPaint(gfx::Canvas* canvas) {
  if (is_hidden_) {
    return;
  }

  // Paint a "hole" in the canvas so that the render of the web page is on
  // top of whatever else has already been painted in the views hierarchy.
  // Later views might still get to paint on top.
  canvas->FillRectInt(SK_ColorBLACK, 0, 0,
                      bounds().width(), bounds().height(),
                      SkXfermode::kClear_Mode);

  // Don't do any painting if the GPU process is rendering directly
  // into the View.
  RenderWidgetHost* render_widget_host = GetRenderWidgetHost();
  if (render_widget_host->is_accelerated_compositing_active()) {
    return;
  }

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

gfx::NativeCursor RenderWidgetHostViewViews::GetCursorForPoint(
    ui::EventType type, const gfx::Point& point) {
  return native_cursor_;
}

bool RenderWidgetHostViewViews::OnMousePressed(const views::MouseEvent& event) {
  RequestFocus();

  // TODO(anicolao): validate event generation.
  WebKit::WebMouseEvent e = WebMouseEventFromViewsEvent(event);

  // TODO(anicolao): deal with double clicks
  e.type = WebKit::WebInputEvent::MouseDown;
  e.clickCount = 1;

  GetRenderWidgetHost()->ForwardMouseEvent(e);
  return true;
}

void RenderWidgetHostViewViews::OnMouseReleased(const views::MouseEvent& event,
                                                bool canceled) {
  WebKit::WebMouseEvent e = WebMouseEventFromViewsEvent(event);

  e.type = WebKit::WebInputEvent::MouseUp;
  e.clickCount = 1;

  GetRenderWidgetHost()->ForwardMouseEvent(e);
}

bool RenderWidgetHostViewViews::OnMouseDragged(const views::MouseEvent& event) {
  OnMouseMoved(event);
  return true;
}

void RenderWidgetHostViewViews::OnMouseMoved(const views::MouseEvent& event) {
  WebKit::WebMouseEvent e = WebMouseEventFromViewsEvent(event);

  e.type = WebKit::WebInputEvent::MouseMove;

  GetRenderWidgetHost()->ForwardMouseEvent(e);
}

void RenderWidgetHostViewViews::OnMouseEntered(const views::MouseEvent& event) {
  // Already generated synthetically by webkit.
}

void RenderWidgetHostViewViews::OnMouseExited(const views::MouseEvent& event) {
  // Already generated synthetically by webkit.
}

bool RenderWidgetHostViewViews::OnMouseWheel(
    const views::MouseWheelEvent& event) {
  WebMouseWheelEvent wmwe;
  InitializeWebMouseEventFromViewsEvent(event, GetMirroredPosition(), &wmwe);

  wmwe.type = WebKit::WebInputEvent::MouseWheel;
  wmwe.button = WebKit::WebMouseEvent::ButtonNone;

  // TODO(sadrul): How do we determine if it's a horizontal scroll?
  wmwe.deltaY = event.offset();
  wmwe.wheelTicksY = wmwe.deltaY > 0 ? 1 : -1;

  GetRenderWidgetHost()->ForwardWheelEvent(wmwe);
  return true;
}

bool RenderWidgetHostViewViews::OnKeyPressed(const views::KeyEvent& event) {
  if (!ime_context_->FilterKeyEvent(event))
    ForwardKeyEvent(event);
  return TRUE;
}

bool RenderWidgetHostViewViews::OnKeyReleased(const views::KeyEvent& event) {
  if (!ime_context_->FilterKeyEvent(event))
    ForwardKeyEvent(event);
  return TRUE;
}

void RenderWidgetHostViewViews::OnFocus() {
  ime_context_->Focus();
  ShowCurrentCursor();
  GetRenderWidgetHost()->GotFocus();
}

void RenderWidgetHostViewViews::OnBlur() {
  // If we are showing a context menu, maintain the illusion that webkit has
  // focus.
  if (!is_showing_context_menu_ && !is_hidden_ && host_)
    host_->Blur();
  ime_context_->Blur();
}


void RenderWidgetHostViewViews::ShowCurrentCursor() {
  // The widget may not have a window. If that's the case, abort mission. This
  // is the same issue as that explained above in Paint().
  if (!GetInnerNativeView() || !GetInnerNativeView()->window)
    return;

  native_cursor_ = current_cursor_.GetNativeCursor();
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

gfx::PluginWindowHandle RenderWidgetHostViewViews::AcquireCompositingSurface() {
  GtkNativeViewManager* manager = GtkNativeViewManager::GetInstance();
  gfx::PluginWindowHandle surface = gfx::kNullPluginWindow;
  gfx::NativeViewId view_id = gfx::IdFromNativeView(GetInnerNativeView());

  if (!manager->GetXIDForId(&surface, view_id)) {
    DLOG(ERROR) << "Can't find XID for view id " << view_id;
  }
  return surface;
}

void RenderWidgetHostViewViews::ReleaseCompositingSurface(
    gfx::PluginWindowHandle surface) {
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

void RenderWidgetHostViewViews::ForwardKeyEvent(
    const views::KeyEvent& event) {
  // This is how it works:
  // (1) If a RawKeyDown event is an accelerator for a reserved command (see
  //     Browser::IsReservedCommand), then the command is executed. Otherwise,
  //     the event is first sent off to the renderer. The renderer is also
  //     notified whether the event would trigger an accelerator in the browser.
  // (2) A Char event is then sent to the renderer.
  // (3) If the renderer does not process the event in step (1), and the event
  //     triggers an accelerator, then it will ignore the event in step (2). The
  //     renderer also sends back notification to the browser for both steps (1)
  //     and (2) about whether the events were processed or not. If the event
  //     for (1) is not processed by the renderer, then it is processed by the
  //     browser, and (2) is ignored.
  if (event.type() == ui::ET_KEY_PRESSED) {
    NativeWebKeyboardEvent wke;

    wke.type = WebKit::WebInputEvent::RawKeyDown;
    wke.windowsKeyCode = event.key_code();
    wke.setKeyIdentifierFromWindowsKeyCode();

    int keyval = ui::GdkKeyCodeForWindowsKeyCode(event.key_code(),
        event.IsShiftDown() ^ event.IsCapsLockDown());

    wke.text[0] = wke.unmodifiedText[0] =
        static_cast<unsigned short>(gdk_keyval_to_unicode(keyval));

    wke.modifiers = WebInputEventFlagsFromViewsEvent(event);

    ForwardWebKeyboardEvent(wke);

    wke.type = WebKit::WebInputEvent::Char;
    ForwardWebKeyboardEvent(wke);
  } else {
    NativeWebKeyboardEvent wke;

    wke.type = WebKit::WebInputEvent::KeyUp;
    wke.windowsKeyCode = event.key_code();
    wke.setKeyIdentifierFromWindowsKeyCode();
    ForwardWebKeyboardEvent(wke);
  }
}

void RenderWidgetHostViewViews::ForwardWebKeyboardEvent(
    const NativeWebKeyboardEvent& event) {
  if (!host_)
    return;

  EditCommands edit_commands;
#if 0
  // TODO(bryeung): key bindings
  if (!event.skip_in_browser &&
      key_bindings_handler_->Match(event, &edit_commands)) {
    host_->ForwardEditCommandsForNextKeyEvent(edit_commands);
  }
#endif

  host_->ForwardKeyboardEvent(event);
}

views::View::TouchStatus RenderWidgetHostViewViews::OnTouchEvent(
    const views::TouchEvent& event) {
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

std::string RenderWidgetHostViewViews::GetClassName() const {
  return kViewClassName;
}

// static
RenderWidgetHostView*
    RenderWidgetHostView::GetRenderWidgetHostViewFromNativeView(
        gfx::NativeView widget) {
  gpointer user_data = g_object_get_data(G_OBJECT(widget),
                                         kRenderWidgetHostViewKey);
  return reinterpret_cast<RenderWidgetHostView*>(user_data);
}
