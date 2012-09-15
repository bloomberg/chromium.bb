// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_widget_host_view_aura.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/string_number_conversions.h"
#include "content/browser/renderer_host/backing_store_aura.h"
#include "content/browser/renderer_host/dip_util.h"
#include "content/browser/renderer_host/render_view_host_delegate.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/web_input_event_aura.h"
#include "content/common/gpu/client/gl_helper.h"
#include "content/common/gpu/gpu_messages.h"
#include "content/common/view_messages.h"
#include "content/port/browser/render_widget_host_view_port.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/common/content_switches.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebCompositionUnderline.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebInputEvent.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebScreenInfo.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/cursor_client.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/client/stacking_client.h"
#include "ui/aura/client/tooltip_client.h"
#include "ui/aura/client/window_types.h"
#include "ui/aura/env.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/base/events/event.h"
#include "ui/base/gestures/gesture_recognizer.h"
#include "ui/base/hit_test.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ui_base_types.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/display.h"
#include "ui/gfx/screen.h"
#include "ui/gfx/skia_util.h"

using WebKit::WebScreenInfo;
using WebKit::WebTouchEvent;

namespace content {
namespace {

// In mouse lock mode, we need to prevent the (invisible) cursor from hitting
// the border of the view, in order to get valid movement information. However,
// forcing the cursor back to the center of the view after each mouse move
// doesn't work well. It reduces the frequency of useful mouse move messages
// significantly. Therefore, we move the cursor to the center of the view only
// if it approaches the border. |kMouseLockBorderPercentage| specifies the width
// of the border area, in percentage of the corresponding dimension.
const int kMouseLockBorderPercentage = 15;

// When accelerated compositing is enabled and a widget resize is pending,
// we delay further resizes of the UI. The following constant is the maximum
// length of time that we should delay further UI resizes while waiting for a
// resized frame from a renderer.
const int kResizeLockTimeoutMs = 67;

ui::TouchStatus DecideTouchStatus(const WebKit::WebTouchEvent& event,
                                  WebKit::WebTouchPoint* point) {
  if (event.type == WebKit::WebInputEvent::TouchEnd &&
      event.touchesLength == 0)
    return ui::TOUCH_STATUS_QUEUED_END;

  return ui::TOUCH_STATUS_QUEUED;
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

bool CanRendererHandleEvent(const ui::MouseEvent* event) {
  if (event->type() == ui::ET_MOUSE_CAPTURE_CHANGED)
    return false;

#if defined(OS_WIN)
  // Renderer cannot handle WM_XBUTTON events.
  switch (event->native_event().message) {
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

void GetScreenInfoForWindow(WebScreenInfo* results, aura::Window* window) {
  const gfx::Display display = window ?
      gfx::Screen::GetDisplayNearestWindow(window) :
      gfx::Screen::GetPrimaryDisplay();
  const gfx::Size size = display.size();
  results->rect = WebKit::WebRect(0, 0, size.width(), size.height());
  results->availableRect = results->rect;
  // TODO(derat|oshima): Don't hardcode this. Get this from display object.
  results->depth = 24;
  results->depthPerComponent = 8;
  int default_dpi = display.device_scale_factor() * 160;
  results->verticalDPI = default_dpi;
  results->horizontalDPI = default_dpi;
}

bool ShouldSendPinchGesture() {
  static bool pinch_allowed =
      CommandLine::ForCurrentProcess()->HasSwitch(switches::kEnableViewport) ||
      CommandLine::ForCurrentProcess()->HasSwitch(switches::kEnablePinch);
  return pinch_allowed;
}

bool ShouldReleaseFrontSurface() {
  static bool release_front_surface_allowed =
      CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableUIReleaseFrontSurface);
  return release_front_surface_allowed;
}

}  // namespace

// We have to implement the WindowObserver interface on a separate object
// because clang doesn't like implementing multiple interfaces that have
// methods with the same name. This object is owned by the
// RenderWidgetHostViewAura.
class RenderWidgetHostViewAura::WindowObserver : public aura::WindowObserver {
 public:
  explicit WindowObserver(RenderWidgetHostViewAura* view) : view_(view) {}
  virtual ~WindowObserver() {}

    // Overridden from aura::WindowObserver:
  virtual void OnWindowRemovingFromRootWindow(aura::Window* window) OVERRIDE {
    view_->RemovingFromRootWindow();
  }

 private:
  RenderWidgetHostViewAura* view_;

  DISALLOW_COPY_AND_ASSIGN(WindowObserver);
};

class RenderWidgetHostViewAura::ResizeLock {
 public:
  ResizeLock(aura::RootWindow* root_window, const gfx::Size new_size)
      : root_window_(root_window),
        new_size_(new_size),
        compositor_lock_(root_window_->GetCompositorLock()),
        weak_ptr_factory_(this) {
    root_window_->HoldMouseMoves();

    BrowserThread::PostDelayedTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&RenderWidgetHostViewAura::ResizeLock::CancelLock,
                   weak_ptr_factory_.GetWeakPtr()),
        base::TimeDelta::FromMilliseconds(kResizeLockTimeoutMs));
  }

  ~ResizeLock() {
    CancelLock();
  }

  void UnlockCompositor() {
    compositor_lock_ = NULL;
  }

  void CancelLock() {
    if (!root_window_)
      return;
    UnlockCompositor();
    root_window_->ReleaseMouseMoves();
    root_window_ = NULL;
  }

  const gfx::Size& expected_size() const {
    return new_size_;
  }

 private:
  aura::RootWindow* root_window_;
  gfx::Size new_size_;
  scoped_refptr<aura::CompositorLock> compositor_lock_;
  base::WeakPtrFactory<ResizeLock> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ResizeLock);
};

////////////////////////////////////////////////////////////////////////////////
// RenderWidgetHostViewAura, public:

RenderWidgetHostViewAura::RenderWidgetHostViewAura(RenderWidgetHost* host)
    : host_(RenderWidgetHostImpl::From(host)),
      ALLOW_THIS_IN_INITIALIZER_LIST(window_(new aura::Window(this))),
      in_shutdown_(false),
      is_fullscreen_(false),
      popup_parent_host_view_(NULL),
      popup_child_host_view_(NULL),
      is_loading_(false),
      text_input_type_(ui::TEXT_INPUT_TYPE_NONE),
      can_compose_inline_(true),
      has_composition_text_(false),
      current_surface_(0),
      current_surface_is_protected_(true),
      current_surface_in_use_by_compositor_(true),
      protection_state_id_(0),
      surface_route_id_(0),
      paint_canvas_(NULL),
      synthetic_move_sent_(false),
      accelerated_compositing_state_changed_(false) {
  host_->SetView(this);
  window_observer_.reset(new WindowObserver(this));
  window_->AddObserver(window_observer_.get());
  aura::client::SetTooltipText(window_, &tooltip_);
  aura::client::SetActivationDelegate(window_, this);
}

////////////////////////////////////////////////////////////////////////////////
// RenderWidgetHostViewAura, RenderWidgetHostView implementation:

void RenderWidgetHostViewAura::InitAsChild(
    gfx::NativeView parent_view) {
  window_->Init(ui::LAYER_TEXTURED);
  window_->SetName("RenderWidgetHostViewAura");
}

void RenderWidgetHostViewAura::InitAsPopup(
    RenderWidgetHostView* parent_host_view,
    const gfx::Rect& bounds_in_display) {
  popup_parent_host_view_ =
      static_cast<RenderWidgetHostViewAura*>(parent_host_view);
  popup_parent_host_view_->popup_child_host_view_ = this;
  window_->SetType(aura::client::WINDOW_TYPE_MENU);
  window_->Init(ui::LAYER_TEXTURED);
  window_->SetName("RenderWidgetHostViewAura");

  aura::Window* parent = NULL;
  aura::RootWindow* root = popup_parent_host_view_->window_->GetRootWindow();
  aura::client::ScreenPositionClient* screen_position_client =
      aura::client::GetScreenPositionClient(root);
  if (screen_position_client) {
    gfx::Point origin_in_screen(bounds_in_display.origin());
    screen_position_client->ConvertPointToScreen(root, &origin_in_screen);
    parent = aura::client::GetStackingClient()->GetDefaultParent(
        window_, gfx::Rect(origin_in_screen, bounds_in_display.size()));
  }
  window_->SetParent(parent);
  SetBounds(bounds_in_display);
  Show();
}

void RenderWidgetHostViewAura::InitAsFullscreen(
    RenderWidgetHostView* reference_host_view) {
  is_fullscreen_ = true;
  window_->SetType(aura::client::WINDOW_TYPE_NORMAL);
  window_->Init(ui::LAYER_TEXTURED);
  window_->SetName("RenderWidgetHostViewAura");
  window_->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_FULLSCREEN);
  aura::Window* parent = NULL;
  if (reference_host_view) {
    aura::Window* reference_window =
        static_cast<RenderWidgetHostViewAura*>(reference_host_view)->window_;
    gfx::Display display =
        gfx::Screen::GetDisplayNearestWindow(reference_window);
    aura::client::StackingClient* stacking_client =
        aura::client::GetStackingClient();
    if (stacking_client)
      parent = stacking_client->GetDefaultParent(window_, display.bounds());
  }
  window_->SetParent(parent);
  Show();
  Focus();
}

RenderWidgetHost* RenderWidgetHostViewAura::GetRenderWidgetHost() const {
  return host_;
}

void RenderWidgetHostViewAura::WasShown() {
  if (!host_->is_hidden())
    return;
  host_->WasShown();

  if (!current_surface_ && host_->is_accelerated_compositing_active() &&
      !released_front_lock_.get()) {
    released_front_lock_ = window_->GetRootWindow()->GetCompositorLock();
  }

  AdjustSurfaceProtection();
}

void RenderWidgetHostViewAura::WasHidden() {
  if (host_->is_hidden())
    return;
  host_->WasHidden();

  released_front_lock_ = NULL;

  if (ShouldReleaseFrontSurface() &&
      host_->is_accelerated_compositing_active()) {
    current_surface_ = 0;
    UpdateExternalTexture();
  }

  AdjustSurfaceProtection();
}

void RenderWidgetHostViewAura::SetSize(const gfx::Size& size) {
  SetBounds(gfx::Rect(window_->bounds().origin(), size));
}

void RenderWidgetHostViewAura::SetBounds(const gfx::Rect& rect) {
  if (window_->bounds().size() != rect.size() &&
      host_->is_accelerated_compositing_active()) {
    aura::RootWindow* root_window = window_->GetRootWindow();
    if (root_window) {
      resize_locks_.push_back(make_linked_ptr(
          new ResizeLock(root_window, rect.size())));
    }
  }
  window_->SetBounds(rect);
  host_->WasResized();
}

gfx::NativeView RenderWidgetHostViewAura::GetNativeView() const {
  return window_;
}

gfx::NativeViewId RenderWidgetHostViewAura::GetNativeViewId() const {
#if defined(OS_WIN)
  HWND window = window_->GetRootWindow()->GetAcceleratedWidget();
  return reinterpret_cast<gfx::NativeViewId>(window);
#else
  return static_cast<gfx::NativeViewId>(NULL);
#endif
}

gfx::NativeViewAccessible RenderWidgetHostViewAura::GetNativeViewAccessible() {
  NOTIMPLEMENTED();
  return static_cast<gfx::NativeViewAccessible>(NULL);
}

void RenderWidgetHostViewAura::MovePluginWindows(
    const gfx::Point& scroll_offset,
    const std::vector<webkit::npapi::WebPluginGeometry>& plugin_window_moves) {
#if defined(OS_WIN)
  // We need to clip the rectangle to the tab's viewport, otherwise we will draw
  // over the browser UI.
  if (!window_->GetRootWindow()) {
    DCHECK(plugin_window_moves.empty());
    return;
  }
  HWND parent = window_->GetRootWindow()->GetAcceleratedWidget();
  gfx::Rect view_bounds = window_->GetBoundsInRootWindow();
  std::vector<webkit::npapi::WebPluginGeometry> moves = plugin_window_moves;

  gfx::Rect view_port(scroll_offset.x(), scroll_offset.y(), view_bounds.width(),
                      view_bounds.height());

  for (size_t i = 0; i < moves.size(); ++i) {
    gfx::Rect clip = moves[i].clip_rect;
    clip.Offset(moves[i].window_rect.origin());
    clip.Offset(scroll_offset);
    clip = clip.Intersect(view_port);
    clip.Offset(-moves[i].window_rect.x(), -moves[i].window_rect.y());
    clip.Offset(-scroll_offset.x(), -scroll_offset.y());
    moves[i].clip_rect = clip;

    moves[i].window_rect.Offset(view_bounds.origin());
  }
  MovePluginWindowsHelper(parent, moves);
#endif  // defined(OS_WIN)
}

void RenderWidgetHostViewAura::Focus() {
  // Make sure we have a FocusManager before attempting to Focus(). In some
  // situations we may not yet be in a valid Window hierarchy (such as reloading
  // after out of memory discared the tab).
  if (window_->GetFocusManager())
    window_->Focus();
}

void RenderWidgetHostViewAura::Blur() {
  window_->Blur();
}

bool RenderWidgetHostViewAura::HasFocus() const {
  return window_->HasFocus();
}

bool RenderWidgetHostViewAura::IsSurfaceAvailableForCopy() const {
  return current_surface_ != 0;
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
  return window_->GetBoundsInRootWindow();
}

void RenderWidgetHostViewAura::UpdateCursor(const WebCursor& cursor) {
  current_cursor_ = cursor;
  const gfx::Display display = gfx::Screen::GetDisplayNearestWindow(window_);
  current_cursor_.SetScaleFactor(display.device_scale_factor());
  UpdateCursorIfOverSelf();
}

void RenderWidgetHostViewAura::SetIsLoading(bool is_loading) {
  is_loading_ = is_loading;
  UpdateCursorIfOverSelf();
}

void RenderWidgetHostViewAura::TextInputStateChanged(
    const ViewHostMsg_TextInputState_Params& params) {
  if (text_input_type_ != params.type ||
      can_compose_inline_ != params.can_compose_inline) {
    text_input_type_ = params.type;
    can_compose_inline_ = params.can_compose_inline;
    if (GetInputMethod())
      GetInputMethod()->OnTextInputTypeChanged(this);
  }
}

void RenderWidgetHostViewAura::ImeCancelComposition() {
  if (GetInputMethod())
    GetInputMethod()->CancelComposition(this);
  has_composition_text_ = false;
}

void RenderWidgetHostViewAura::ImeCompositionRangeChanged(
    const ui::Range& range,
    const std::vector<gfx::Rect>& character_bounds) {
  composition_character_bounds_ = character_bounds;
}

void RenderWidgetHostViewAura::DidUpdateBackingStore(
    const gfx::Rect& scroll_rect, int scroll_dx, int scroll_dy,
    const std::vector<gfx::Rect>& copy_rects) {
  if (accelerated_compositing_state_changed_)
    UpdateExternalTexture();

  // Use the state of the RenderWidgetHost and not the window as the two may
  // differ. In particular if the window is hidden but the renderer isn't and we
  // ignore the update and the window is made visible again the layer isn't
  // marked as dirty and we show the wrong thing.
  // We do this after UpdateExternalTexture() so that when we become visible
  // we're not drawing a stale texture.
  if (host_->is_hidden())
    return;

  gfx::Rect clip_rect;
  if (paint_canvas_) {
    SkRect sk_clip_rect;
    if (paint_canvas_->sk_canvas()->getClipBounds(&sk_clip_rect))
      clip_rect = gfx::SkRectToRect(sk_clip_rect);
  }

  if (!scroll_rect.IsEmpty())
    SchedulePaintIfNotInClip(scroll_rect, clip_rect);

  for (size_t i = 0; i < copy_rects.size(); ++i) {
    gfx::Rect rect = copy_rects[i].Subtract(scroll_rect);
    if (rect.IsEmpty())
      continue;

    SchedulePaintIfNotInClip(rect, clip_rect);

#if defined(OS_WIN)
    // Send the invalid rect in screen coordinates.
    gfx::Rect screen_rect = GetViewBounds();
    gfx::Rect invalid_screen_rect(rect);
    invalid_screen_rect.Offset(screen_rect.x(), screen_rect.y());
    HWND hwnd = window_->GetRootWindow()->GetAcceleratedWidget();
    PaintPluginWindowsHelper(hwnd, invalid_screen_rect);
#endif  // defined(OS_WIN)
  }
}

void RenderWidgetHostViewAura::RenderViewGone(base::TerminationStatus status,
                                              int error_code) {
  UpdateCursorIfOverSelf();
  Destroy();
}

void RenderWidgetHostViewAura::Destroy() {
  // Beware, this function is not called on all destruction paths. It will
  // implicitly end up calling ~RenderWidgetHostViewAura though, so all
  // destruction/cleanup code should happen there, not here.
  in_shutdown_ = true;
  delete window_;
}

void RenderWidgetHostViewAura::SetTooltipText(const string16& tooltip_text) {
  tooltip_ = tooltip_text;
  aura::RootWindow* root_window = window_->GetRootWindow();
  if (aura::client::GetTooltipClient(root_window))
    aura::client::GetTooltipClient(root_window)->UpdateTooltip(window_);
}

void RenderWidgetHostViewAura::SelectionBoundsChanged(
    const gfx::Rect& start_rect,
    WebKit::WebTextDirection start_direction,
    const gfx::Rect& end_rect,
    WebKit::WebTextDirection end_direction) {
  if (selection_start_rect_ == start_rect && selection_end_rect_ == end_rect)
    return;

  selection_start_rect_ = start_rect;
  selection_end_rect_ = end_rect;

  if (GetInputMethod())
    GetInputMethod()->OnCaretBoundsChanged(this);
}

BackingStore* RenderWidgetHostViewAura::AllocBackingStore(
    const gfx::Size& size) {
  return new BackingStoreAura(host_, size);
}

void RenderWidgetHostViewAura::CopyFromCompositingSurface(
    const gfx::Rect& src_subrect,
    const gfx::Size& dst_size,
    const base::Callback<void(bool)>& callback,
    skia::PlatformCanvas* output) {
  base::ScopedClosureRunner scoped_callback_runner(base::Bind(callback, false));

  std::map<uint64, scoped_refptr<ui::Texture> >::iterator it =
      image_transport_clients_.find(current_surface_);
  if (it == image_transport_clients_.end())
    return;

  ui::Texture* container = it->second;
  DCHECK(container);

  gfx::Size dst_size_in_pixel = ConvertSizeToPixel(this, dst_size);
  if (!output->initialize(
      dst_size_in_pixel.width(), dst_size_in_pixel.height(), true))
    return;

  ImageTransportFactory* factory = ImageTransportFactory::GetInstance();
  GLHelper* gl_helper = factory->GetGLHelper();
  if (!gl_helper)
    return;

  unsigned char* addr = static_cast<unsigned char*>(
      output->getTopDevice()->accessBitmap(true).getPixels());
  scoped_callback_runner.Release();
  // Wrap the callback with an internal handler so that we can inject our
  // own completion handlers (where we can call AdjustSurfaceProtection).
  base::Callback<void(bool)> wrapper_callback = base::Bind(
      &RenderWidgetHostViewAura::CopyFromCompositingSurfaceFinished,
      AsWeakPtr(),
      callback);
  pending_thumbnail_tasks_.push_back(callback);

  // Convert |src_subrect| from the views coordinate (upper-left origin) into
  // the OpenGL coordinate (lower-left origin).
  gfx::Rect src_subrect_in_gl = src_subrect;
  src_subrect_in_gl.set_y(GetViewBounds().height() - src_subrect.bottom());

  gfx::Rect src_subrect_in_pixel = ConvertRectToPixel(this, src_subrect_in_gl);
  gl_helper->CopyTextureTo(container->texture_id(),
                           container->size(),
                           src_subrect_in_pixel,
                           dst_size_in_pixel,
                           addr,
                           wrapper_callback);
}

void RenderWidgetHostViewAura::OnAcceleratedCompositingStateChange() {
  // Delay processing the state change until we either get a software frame if
  // switching to software mode or receive a buffers swapped notification
  // if switching to accelerated mode.
  // Sometimes (e.g. on a page load) the renderer will spuriously disable then
  // re-enable accelerated compositing, causing us to flash.
  // TODO(piman): factor the enable/disable accelerated compositing message into
  // the UpdateRect/AcceleratedSurfaceBuffersSwapped messages so that we have
  // fewer inconsistent temporary states.
  accelerated_compositing_state_changed_ = true;
}

void RenderWidgetHostViewAura::UpdateExternalTexture() {
  // Delay processing accelerated compositing state change till here where we
  // act upon the state change. (Clear the external texture if switching to
  // software mode or set the external texture if going to accelerated mode).
  if (accelerated_compositing_state_changed_) {
    // Don't scale the contents in accelerated mode because the renderer takes
    // care of it.
    window_->layer()->set_scale_content(
        !host_->is_accelerated_compositing_active());

    accelerated_compositing_state_changed_ = false;
  }

  if (current_surface_ != 0 && host_->is_accelerated_compositing_active()) {
    ui::Texture* container = image_transport_clients_[current_surface_];
    window_->SetExternalTexture(container);
    current_surface_in_use_by_compositor_ = true;

    if (!container) {
      resize_locks_.clear();
    } else {
      typedef std::vector<linked_ptr<ResizeLock> > ResizeLockList;
      ResizeLockList::iterator it = resize_locks_.begin();
      while (it != resize_locks_.end()) {
        gfx::Size container_size = ConvertSizeToDIP(this,
            container->size());
        if ((*it)->expected_size() == container_size)
          break;
        ++it;
      }
      if (it != resize_locks_.end()) {
        ++it;
        ui::Compositor* compositor = GetCompositor();
        if (compositor) {
          // Delay the release of the lock until we've kicked a frame with the
          // new texture, to avoid resizing the UI before we have a chance to
          // draw a "good" frame.
          locks_pending_draw_.insert(
              locks_pending_draw_.begin(), resize_locks_.begin(), it);
          // However since we got the size we were looking for, unlock the
          // compositor.
          for (ResizeLockList::iterator it2 = resize_locks_.begin();
              it2 !=it; ++it2) {
            it2->get()->UnlockCompositor();
          }
          if (!compositor->HasObserver(this))
            compositor->AddObserver(this);
        }
        resize_locks_.erase(resize_locks_.begin(), it);
      }
    }
  } else {
    window_->SetExternalTexture(NULL);
    if (ShouldReleaseFrontSurface() &&
        host_->is_accelerated_compositing_active()) {
      // The current surface may have pipelined gl commands, so always wait for
      // the next composite to start.  If the current surface is still null,
      // then we really know its no longer in use.
      ui::Compositor* compositor = GetCompositor();
      if (compositor) {
        on_compositing_will_start_callbacks_.push_back(
            base::Bind(&RenderWidgetHostViewAura::
                           SetSurfaceNotInUseByCompositor,
                       AsWeakPtr()));
        if (!compositor->HasObserver(this))
          compositor->AddObserver(this);
      }
    }
    resize_locks_.clear();
  }
}

void RenderWidgetHostViewAura::AcceleratedSurfaceBuffersSwapped(
    const GpuHostMsg_AcceleratedSurfaceBuffersSwapped_Params& params_in_pixel,
    int gpu_host_id) {
  surface_route_id_ = params_in_pixel.route_id;
  // If protection state changed, then this swap is stale. We must still ACK but
  // do not update current_surface_ since it may have been discarded.
  if (params_in_pixel.protection_state_id &&
      params_in_pixel.protection_state_id != protection_state_id_) {
    DCHECK(!current_surface_);
    if (!params_in_pixel.skip_ack)
      InsertSyncPointAndACK(params_in_pixel.route_id, gpu_host_id, NULL);
    return;
  }
  current_surface_ = params_in_pixel.surface_handle;
  // If we don't require an ACK that means the content is not a fresh updated
  // new frame, rather we are just resetting our handle to some old content that
  // we still hadn't discarded. Although we could display immediately, by not
  // resetting the compositor lock here, we give us some time to get a fresh
  // frame which means fewer content flashes.
  if (!params_in_pixel.skip_ack)
    released_front_lock_ = NULL;

  UpdateExternalTexture();

  ui::Compositor* compositor = GetCompositor();
  if (!compositor) {
    // We have no compositor, so we have no way to display the surface.
    // Must still send the ACK.
    if (!params_in_pixel.skip_ack)
      InsertSyncPointAndACK(params_in_pixel.route_id, gpu_host_id, NULL);
  } else {
    DCHECK(image_transport_clients_.find(params_in_pixel.surface_handle) !=
           image_transport_clients_.end());
    gfx::Size surface_size_in_pixel =
        image_transport_clients_[params_in_pixel.surface_handle]->size();
    gfx::Size surface_size = ConvertSizeToDIP(this,
                                                       surface_size_in_pixel);
    window_->SchedulePaintInRect(gfx::Rect(surface_size));

    if (!params_in_pixel.skip_ack) {
      if (!resize_locks_.empty()) {
        // If we are waiting for the resize, fast-track the ACK.
        if (compositor->IsThreaded()) {
          // We need the compositor thread to pick up the active buffer before
          // ACKing.
          on_compositing_did_commit_callbacks_.push_back(
              base::Bind(&RenderWidgetHostViewAura::InsertSyncPointAndACK,
                         params_in_pixel.route_id,
                         gpu_host_id));
          if (!compositor->HasObserver(this))
            compositor->AddObserver(this);
        } else {
          // The compositor will pickup the active buffer during a draw, so we
          // can ACK immediately.
          InsertSyncPointAndACK(params_in_pixel.route_id, gpu_host_id,
                                compositor);
        }
      } else {
        // Add sending an ACK to the list of things to do OnCompositingWillStart
        on_compositing_will_start_callbacks_.push_back(
            base::Bind(&RenderWidgetHostViewAura::InsertSyncPointAndACK,
                       params_in_pixel.route_id,
                       gpu_host_id));
        if (!compositor->HasObserver(this))
          compositor->AddObserver(this);
      }
    }
  }
}

void RenderWidgetHostViewAura::AcceleratedSurfacePostSubBuffer(
    const GpuHostMsg_AcceleratedSurfacePostSubBuffer_Params& params_in_pixel,
    int gpu_host_id) {
  surface_route_id_ = params_in_pixel.route_id;
  // If visible state changed, then this PSB is stale. We must still ACK but
  // do not update current_surface_.
  if (params_in_pixel.protection_state_id &&
      params_in_pixel.protection_state_id != protection_state_id_) {
    DCHECK(!current_surface_);
    InsertSyncPointAndACK(params_in_pixel.route_id, gpu_host_id, NULL);
    return;
  }
  current_surface_ = params_in_pixel.surface_handle;
  released_front_lock_ = NULL;
  DCHECK(current_surface_);
  UpdateExternalTexture();

  ui::Compositor* compositor = GetCompositor();
  if (!compositor) {
    // We have no compositor, so we have no way to display the surface
    // Must still send the ACK
    InsertSyncPointAndACK(params_in_pixel.route_id, gpu_host_id, NULL);
  } else {
    DCHECK(image_transport_clients_.find(params_in_pixel.surface_handle) !=
           image_transport_clients_.end());
    gfx::Size surface_size_in_pixel =
        image_transport_clients_[params_in_pixel.surface_handle]->size();

    // Co-ordinates come in OpenGL co-ordinate space.
    // We need to convert to layer space.
    gfx::Rect rect_to_paint = ConvertRectToDIP(this, gfx::Rect(
        params_in_pixel.x,
        surface_size_in_pixel.height() - params_in_pixel.y -
            params_in_pixel.height,
        params_in_pixel.width,
        params_in_pixel.height));

    // Damage may not have been DIP aligned, so inflate damage to compensate
    // for any round-off error.
    rect_to_paint.Inset(-1, -1);
    rect_to_paint = rect_to_paint.Intersect(window_->bounds());

    window_->SchedulePaintInRect(rect_to_paint);

    if (!resize_locks_.empty()) {
      // If we are waiting for the resize, fast-track the ACK.
      if (compositor->IsThreaded()) {
        // We need the compositor thread to pick up the active buffer before
        // ACKing.
        on_compositing_did_commit_callbacks_.push_back(
            base::Bind(&RenderWidgetHostViewAura::InsertSyncPointAndACK,
                       params_in_pixel.route_id,
                       gpu_host_id));
        if (!compositor->HasObserver(this))
          compositor->AddObserver(this);
      } else {
        // The compositor will pickup the active buffer during a draw, so we
        // can ACK immediately.
        InsertSyncPointAndACK(params_in_pixel.route_id, gpu_host_id,
                              compositor);
      }
    } else {
      // Add sending an ACK to the list of things to do OnCompositingWillStart
      on_compositing_will_start_callbacks_.push_back(
          base::Bind(&RenderWidgetHostViewAura::InsertSyncPointAndACK,
                     params_in_pixel.route_id,
                     gpu_host_id));
      if (!compositor->HasObserver(this))
        compositor->AddObserver(this);
    }
  }
}

void RenderWidgetHostViewAura::AcceleratedSurfaceSuspend() {
}

bool RenderWidgetHostViewAura::HasAcceleratedSurface(
      const gfx::Size& desired_size) {
  // Aura doesn't use GetBackingStore for accelerated pages, so it doesn't
  // matter what is returned here as GetBackingStore is the only caller of this
  // method. TODO(jbates) implement this if other Aura code needs it.
  return false;
}

// TODO(backer): Drop the |shm_handle| once I remove some unused service side
// code.
void RenderWidgetHostViewAura::AcceleratedSurfaceNew(
      int32 width_in_pixel,
      int32 height_in_pixel,
      uint64 surface_handle) {
  ImageTransportFactory* factory = ImageTransportFactory::GetInstance();
  scoped_refptr<ui::Texture> surface(factory->CreateTransportClient(
      gfx::Size(width_in_pixel, height_in_pixel), surface_handle));
  if (!surface) {
    LOG(ERROR) << "Failed to create ImageTransport texture";
    return;
  }

  image_transport_clients_[surface_handle] = surface;
}

void RenderWidgetHostViewAura::AcceleratedSurfaceRelease(
    uint64 surface_handle) {
  DCHECK(image_transport_clients_.find(surface_handle) !=
         image_transport_clients_.end());
  if (current_surface_ == surface_handle) {
    current_surface_ = 0;
    UpdateExternalTexture();
  }
  image_transport_clients_.erase(surface_handle);
}

void RenderWidgetHostViewAura::SetSurfaceNotInUseByCompositor(ui::Compositor*) {
  if (current_surface_ || !host_->is_hidden())
    return;
  current_surface_in_use_by_compositor_ = false;
  AdjustSurfaceProtection();
}

void RenderWidgetHostViewAura::AdjustSurfaceProtection() {
  // If the current surface is non null, it is protected.
  // If we are visible, it is protected.
  // Otherwise, change to not proctected once done thumbnailing and compositing.
  bool surface_is_protected =
      current_surface_ ||
      !host_->is_hidden() ||
      (current_surface_is_protected_ &&
          (!pending_thumbnail_tasks_.empty() ||
              current_surface_in_use_by_compositor_));
  if (current_surface_is_protected_ == surface_is_protected)
    return;
  current_surface_is_protected_ = surface_is_protected;
  ++protection_state_id_;

  if (!surface_route_id_ || !shared_surface_handle_.parent_gpu_process_id)
    return;

  RenderWidgetHostImpl::SendFrontSurfaceIsProtected(
      surface_is_protected,
      protection_state_id_,
      surface_route_id_,
      shared_surface_handle_.parent_gpu_process_id);
}

void RenderWidgetHostViewAura::CopyFromCompositingSurfaceFinished(
    base::Callback<void(bool)> callback, bool result) {
  for (size_t i = 0; i != pending_thumbnail_tasks_.size(); ++i) {
    if (pending_thumbnail_tasks_[i].Equals(callback)) {
      pending_thumbnail_tasks_.erase(pending_thumbnail_tasks_.begin()+i);
      break;
    }
  }
  AdjustSurfaceProtection();
  callback.Run(result);
}

void RenderWidgetHostViewAura::SetBackground(const SkBitmap& background) {
  RenderWidgetHostViewBase::SetBackground(background);
  host_->SetBackground(background);
  window_->layer()->SetFillsBoundsOpaquely(background.isOpaque());
}

void RenderWidgetHostViewAura::GetScreenInfo(WebScreenInfo* results) {
  GetScreenInfoForWindow(results, window_);
}

gfx::Rect RenderWidgetHostViewAura::GetBoundsInRootWindow() {
  return window_->GetToplevelWindow()->GetBoundsInRootWindow();
}

void RenderWidgetHostViewAura::ProcessTouchAck(
    WebKit::WebInputEvent::Type type, bool processed) {
  // The ACKs for the touch-events arrive in the same sequence as they were
  // dispatched.
  aura::RootWindow* root_window = window_->GetRootWindow();
  if (root_window)
    root_window->AdvanceQueuedTouchEvent(window_, processed);
}

void RenderWidgetHostViewAura::SetHasHorizontalScrollbar(
    bool has_horizontal_scrollbar) {
  // Not needed. Mac-only.
}

void RenderWidgetHostViewAura::SetScrollOffsetPinning(
    bool is_pinned_to_left, bool is_pinned_to_right) {
  // Not needed. Mac-only.
}

gfx::GLSurfaceHandle RenderWidgetHostViewAura::GetCompositingSurface() {
  if (shared_surface_handle_.is_null()) {
    ImageTransportFactory* factory = ImageTransportFactory::GetInstance();
    shared_surface_handle_ = factory->CreateSharedSurfaceHandle();
    factory->AddObserver(this);
  }
  return shared_surface_handle_;
}

bool RenderWidgetHostViewAura::LockMouse() {
  aura::RootWindow* root_window = window_->GetRootWindow();
  if (!root_window)
    return false;

  if (mouse_locked_)
    return true;

  mouse_locked_ = true;
  window_->SetCapture();
  aura::client::CursorClient* cursor_client =
      aura::client::GetCursorClient(root_window);
  if (cursor_client)
    cursor_client->ShowCursor(false);
  synthetic_move_sent_ = true;
  window_->MoveCursorTo(gfx::Rect(window_->bounds().size()).CenterPoint());
  if (aura::client::GetTooltipClient(root_window))
    aura::client::GetTooltipClient(root_window)->SetTooltipsEnabled(false);
  return true;
}

void RenderWidgetHostViewAura::UnlockMouse() {
  aura::RootWindow* root_window = window_->GetRootWindow();
  if (!mouse_locked_ || !root_window)
    return;

  mouse_locked_ = false;

  window_->ReleaseCapture();
  window_->MoveCursorTo(unlocked_mouse_position_);
  aura::client::CursorClient* cursor_client =
      aura::client::GetCursorClient(root_window);
  if (cursor_client)
    cursor_client->ShowCursor(true);
  if (aura::client::GetTooltipClient(root_window))
    aura::client::GetTooltipClient(root_window)->SetTooltipsEnabled(true);

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
  if (popup_child_host_view_ && popup_child_host_view_->NeedsInputGrab()) {
    popup_child_host_view_->InsertChar(ch, flags);
    return;
  }

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

bool RenderWidgetHostViewAura::CanComposeInline() const {
  return can_compose_inline_;
}

gfx::Rect RenderWidgetHostViewAura::ConvertRectToScreen(const gfx::Rect& rect) {
  gfx::Point origin = rect.origin();
  gfx::Point end = gfx::Point(rect.right(), rect.bottom());

  aura::RootWindow* root_window = window_->GetRootWindow();
  aura::client::ScreenPositionClient* screen_position_client =
      aura::client::GetScreenPositionClient(root_window);
  screen_position_client->ConvertPointToScreen(window_, &origin);
  screen_position_client->ConvertPointToScreen(window_, &end);
  return gfx::Rect(origin.x(),
                   origin.y(),
                   end.x() - origin.x(),
                   end.y() - origin.y());
}

gfx::Rect RenderWidgetHostViewAura::GetCaretBounds() {
  const gfx::Rect rect = selection_start_rect_.Union(selection_end_rect_);
  return ConvertRectToScreen(rect);
}

bool RenderWidgetHostViewAura::GetCompositionCharacterBounds(uint32 index,
                                                             gfx::Rect* rect) {
  DCHECK(rect);
  if (index >= composition_character_bounds_.size())
    return false;
  *rect = ConvertRectToScreen(composition_character_bounds_[index]);
  return true;
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

  if (GetInputMethod())
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

void RenderWidgetHostViewAura::ExtendSelectionAndDelete(
    size_t before, size_t after) {
  // TODO(horo): implement this method if it is required.
  // http://crbug.com/149155
  NOTIMPLEMENTED();
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

void RenderWidgetHostViewAura::OnFocus(aura::Window* old_focused_window) {
  // We need to honor input bypass if the associated tab is does not want input.
  // This gives the current focused window a chance to be the text input
  // client and handle events.
  if (host_->ignore_input_events())
    return;

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

  DetachFromInputMethod();
  host_->SetInputMethodActive(false);

  // If we lose the focus while fullscreen, close the window; Pepper Flash won't
  // do it for us (unlike NPAPI Flash).
  if (is_fullscreen_ && !in_shutdown_) {
    in_shutdown_ = true;
    host_->Shutdown();
  }
}

gfx::NativeCursor RenderWidgetHostViewAura::GetCursor(const gfx::Point& point) {
  if (mouse_locked_)
    return ui::kCursorNone;
  return current_cursor_.GetNativeCursor();
}

int RenderWidgetHostViewAura::GetNonClientComponent(
    const gfx::Point& point) const {
  return HTCLIENT;
}

bool RenderWidgetHostViewAura::ShouldDescendIntoChildForEventHandling(
    aura::Window* child,
    const gfx::Point& location) {
  return true;
}

bool RenderWidgetHostViewAura::CanFocus() {
  return popup_type_ == WebKit::WebPopupTypeNone;
}

void RenderWidgetHostViewAura::OnCaptureLost() {
  host_->LostCapture();
}

void RenderWidgetHostViewAura::OnPaint(gfx::Canvas* canvas) {
  paint_canvas_ = canvas;
  BackingStore* backing_store = host_->GetBackingStore(true);
  paint_canvas_ = NULL;
  if (backing_store) {
    static_cast<BackingStoreAura*>(backing_store)->SkiaShowRect(gfx::Point(),
                                                                canvas);
  } else if (aura::Env::GetInstance()->render_white_bg()) {
    canvas->DrawColor(SK_ColorWHITE);
  }
}

void RenderWidgetHostViewAura::OnDeviceScaleFactorChanged(
    float device_scale_factor) {
  if (!host_)
    return;

  BackingStoreAura* backing_store = static_cast<BackingStoreAura*>(
      host_->GetBackingStore(false));
  if (backing_store)  // NULL in hardware path.
    backing_store->ScaleFactorChanged(device_scale_factor);

  host_->SetDeviceScaleFactor(device_scale_factor);
  current_cursor_.SetScaleFactor(device_scale_factor);
}

void RenderWidgetHostViewAura::OnWindowDestroying() {
#if defined(OS_WIN)
  if (window_->GetRootWindow()) {
    HWND parent = window_->GetRootWindow()->GetAcceleratedWidget();
    DetachPluginsHelper(parent);
  }
#endif
}

void RenderWidgetHostViewAura::OnWindowDestroyed() {
  host_->ViewDestroyed();
  delete this;
}

void RenderWidgetHostViewAura::OnWindowTargetVisibilityChanged(bool visible) {
}

bool RenderWidgetHostViewAura::HasHitTestMask() const {
  return false;
}

void RenderWidgetHostViewAura::GetHitTestMask(gfx::Path* mask) const {
}

scoped_refptr<ui::Texture> RenderWidgetHostViewAura::CopyTexture() {
  if (!host_->is_accelerated_compositing_active())
    return scoped_refptr<ui::Texture>();

  ImageTransportFactory* factory = ImageTransportFactory::GetInstance();
  GLHelper* gl_helper = factory->GetGLHelper();
  if (!gl_helper)
    return scoped_refptr<ui::Texture>();

  std::map<uint64, scoped_refptr<ui::Texture> >::iterator it =
      image_transport_clients_.find(current_surface_);
  if (it == image_transport_clients_.end())
    return scoped_refptr<ui::Texture>();

  ui::Texture* container = it->second;
  DCHECK(container);
  WebKit::WebGLId texture_id =
      gl_helper->CopyTexture(container->texture_id(), container->size());
  if (!texture_id)
    return scoped_refptr<ui::Texture>();

  return scoped_refptr<ui::Texture>(
      factory->CreateOwnedTexture(container->size(), texture_id));
}

////////////////////////////////////////////////////////////////////////////////
// RenderWidgetHostViewAura, ui::EventHandler implementation:

ui::EventResult RenderWidgetHostViewAura::OnKeyEvent(ui::KeyEvent* event) {
  TRACE_EVENT0("browser", "RenderWidgetHostViewAura::OnKeyEvent");
  if (popup_child_host_view_ && popup_child_host_view_->NeedsInputGrab() &&
      popup_child_host_view_->OnKeyEvent(event))
    return ui::ER_HANDLED;

  // We need to handle the Escape key for Pepper Flash.
  if (is_fullscreen_ && event->key_code() == ui::VKEY_ESCAPE) {
    if (!in_shutdown_) {
      in_shutdown_ = true;
      host_->Shutdown();
    }
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
  return ui::ER_HANDLED;
}

ui::EventResult RenderWidgetHostViewAura::OnMouseEvent(ui::MouseEvent* event) {
  TRACE_EVENT0("browser", "RenderWidgetHostViewAura::OnMouseEvent");
  if (mouse_locked_) {
    WebKit::WebMouseEvent mouse_event = MakeWebMouseEvent(event);
    gfx::Point center(gfx::Rect(window_->bounds().size()).CenterPoint());

    bool is_move_to_center_event = (event->type() == ui::ET_MOUSE_MOVED ||
        event->type() == ui::ET_MOUSE_DRAGGED) &&
        mouse_event.x == center.x() && mouse_event.y == center.y();

    ModifyEventMovementAndCoords(&mouse_event);

    bool should_not_forward = is_move_to_center_event && synthetic_move_sent_;
    if (should_not_forward) {
      synthetic_move_sent_ = false;
    } else {
      // Check if the mouse has reached the border and needs to be centered.
      if (ShouldMoveToCenter()) {
        synthetic_move_sent_ = true;
        window_->MoveCursorTo(center);
      }

      // Forward event to renderer.
      if (CanRendererHandleEvent(event))
        host_->ForwardMouseEvent(mouse_event);
    }

    return ui::ER_UNHANDLED;
  }

  if (event->type() == ui::ET_MOUSEWHEEL) {
    WebKit::WebMouseWheelEvent mouse_wheel_event =
        MakeWebMouseWheelEvent(static_cast<ui::MouseWheelEvent*>(event));
    if (mouse_wheel_event.deltaX != 0 || mouse_wheel_event.deltaY != 0)
      host_->ForwardWheelEvent(mouse_wheel_event);
  } else if (event->type() == ui::ET_SCROLL) {
    WebKit::WebGestureEvent gesture_event =
        MakeWebGestureEventFlingCancel();
    host_->ForwardGestureEvent(gesture_event);
    WebKit::WebMouseWheelEvent mouse_wheel_event =
        MakeWebMouseWheelEvent(static_cast<ui::ScrollEvent*>(event));
    host_->ForwardWheelEvent(mouse_wheel_event);
    RecordAction(UserMetricsAction("TrackpadScroll"));
  } else if (event->type() == ui::ET_SCROLL_FLING_START ||
      event->type() == ui::ET_SCROLL_FLING_CANCEL) {
    WebKit::WebGestureEvent gesture_event =
        MakeWebGestureEvent(static_cast<ui::ScrollEvent*>(event));
    host_->ForwardGestureEvent(gesture_event);
    if (event->type() == ui::ET_SCROLL_FLING_START)
      RecordAction(UserMetricsAction("TrackpadScrollFling"));
  } else if (CanRendererHandleEvent(event)) {
    WebKit::WebMouseEvent mouse_event = MakeWebMouseEvent(event);
    ModifyEventMovementAndCoords(&mouse_event);
    host_->ForwardMouseEvent(mouse_event);
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
  return ui::ER_HANDLED;
}

ui::TouchStatus RenderWidgetHostViewAura::OnTouchEvent(
    ui::TouchEvent* event) {
  TRACE_EVENT0("browser", "RenderWidgetHostViewAura::OnTouchEvent");
  // Update the touch event first.
  WebKit::WebTouchPoint* point = UpdateWebTouchEvent(event,
      &touch_event_);

  // Forward the touch event only if a touch point was updated, and there's a
  // touch-event handler in the page.
  if (point && host_->has_touch_handler()) {
    host_->ForwardTouchEvent(touch_event_);
    UpdateWebTouchEventAfterDispatch(&touch_event_, point);
    return DecideTouchStatus(touch_event_, point);
  }

  return ui::TOUCH_STATUS_UNKNOWN;
}

ui::EventResult RenderWidgetHostViewAura::OnGestureEvent(
    ui::GestureEvent* event) {
  TRACE_EVENT0("browser", "RenderWidgetHostViewAura::OnGestureEvent");
  // Pinch gestures are currently disabled by default. See crbug.com/128477.
  if ((event->type() == ui::ET_GESTURE_PINCH_BEGIN ||
      event->type() == ui::ET_GESTURE_PINCH_UPDATE ||
      event->type() == ui::ET_GESTURE_PINCH_END) && !ShouldSendPinchGesture()) {
    return ui::ER_CONSUMED;
  }

  RenderViewHostDelegate* delegate = NULL;
  if (popup_type_ == WebKit::WebPopupTypeNone && !is_fullscreen_)
    delegate = RenderViewHost::From(host_)->GetDelegate();
  if (delegate && event->type() == ui::ET_GESTURE_BEGIN &&
      event->details().touch_points() == 1) {
    delegate->HandleGestureBegin();
  }

  WebKit::WebGestureEvent gesture = MakeWebGestureEvent(event);
  if (event->type() == ui::ET_GESTURE_TAP_DOWN) {
    // Webkit does not stop a fling-scroll on tap-down. So explicitly send an
    // event to stop any in-progress flings.
    WebKit::WebGestureEvent fling_cancel = gesture;
    fling_cancel.type = WebKit::WebInputEvent::GestureFlingCancel;
    host_->ForwardGestureEvent(fling_cancel);
  }

  if (gesture.type != WebKit::WebInputEvent::Undefined) {
    host_->ForwardGestureEvent(gesture);

    if (event->type() == ui::ET_GESTURE_SCROLL_BEGIN ||
        event->type() == ui::ET_GESTURE_SCROLL_UPDATE ||
        event->type() == ui::ET_GESTURE_SCROLL_END) {
      RecordAction(UserMetricsAction("TouchscreenScroll"));
    } else if (event->type() == ui::ET_SCROLL_FLING_START) {
      RecordAction(UserMetricsAction("TouchscreenScrollFling"));
    }
  }

  if (delegate && event->type() == ui::ET_GESTURE_END &&
      event->details().touch_points() == 1) {
    delegate->HandleGestureEnd();
  }

  // If a gesture is not processed by the webpage, then WebKit processes it
  // (e.g. generates synthetic mouse events). So CONSUMED should be returned
  // from here to avoid any duplicate synthetic mouse-events being generated
  // from aura.
  return ui::ER_CONSUMED;
}

////////////////////////////////////////////////////////////////////////////////
// RenderWidgetHostViewAura, aura::client::ActivationDelegate implementation:

bool RenderWidgetHostViewAura::ShouldActivate(const ui::Event* event) {
  bool activate = false;
  if (event) {
    if (event->type() == ui::ET_MOUSE_PRESSED) {
      activate = true;
    } else if (event->type() == ui::ET_GESTURE_BEGIN) {
      activate = static_cast<const ui::GestureEvent*>(event)->
          details().touch_points() == 1;
    }
  }
  if (activate)
    host_->OnPointerEventActivate();
  return is_fullscreen_;
}

void RenderWidgetHostViewAura::OnActivated() {
}

void RenderWidgetHostViewAura::OnLostActive() {
}

////////////////////////////////////////////////////////////////////////////////
// RenderWidgetHostViewAura, ui::CompositorObserver implementation:

void RenderWidgetHostViewAura::OnCompositingDidCommit(
    ui::Compositor* compositor) {
  RunCompositingDidCommitCallbacks(compositor);
}

void RenderWidgetHostViewAura::OnCompositingWillStart(
    ui::Compositor* compositor) {
  RunCompositingWillStartCallbacks(compositor);
}

void RenderWidgetHostViewAura::OnCompositingStarted(
    ui::Compositor* compositor) {
  locks_pending_draw_.clear();
}

void RenderWidgetHostViewAura::OnCompositingEnded(
    ui::Compositor* compositor) {
}

void RenderWidgetHostViewAura::OnCompositingAborted(
    ui::Compositor* compositor) {
}

////////////////////////////////////////////////////////////////////////////////
// RenderWidgetHostViewAura, ImageTransportFactoryObserver implementation:

void RenderWidgetHostViewAura::OnLostResources() {
  image_transport_clients_.clear();
  current_surface_ = 0;
  protection_state_id_ = 0;
  current_surface_is_protected_ = true;
  current_surface_in_use_by_compositor_ = true;
  surface_route_id_ = 0;
  UpdateExternalTexture();
  locks_pending_draw_.clear();

  DCHECK(!shared_surface_handle_.is_null());
  ImageTransportFactory* factory = ImageTransportFactory::GetInstance();
  factory->DestroySharedSurfaceHandle(shared_surface_handle_);
  shared_surface_handle_ = factory->CreateSharedSurfaceHandle();
  host_->CompositingSurfaceUpdated();
  host_->ScheduleComposite();
}

////////////////////////////////////////////////////////////////////////////////
// RenderWidgetHostViewAura, private:

RenderWidgetHostViewAura::~RenderWidgetHostViewAura() {
  if (!shared_surface_handle_.is_null()) {
    ImageTransportFactory* factory = ImageTransportFactory::GetInstance();
    factory->DestroySharedSurfaceHandle(shared_surface_handle_);
    factory->RemoveObserver(this);
  }
  window_->RemoveObserver(window_observer_.get());
  UnlockMouse();
  if (popup_type_ != WebKit::WebPopupTypeNone) {
    DCHECK(popup_parent_host_view_);
    popup_parent_host_view_->popup_child_host_view_ = NULL;
  }
  aura::client::SetTooltipText(window_, NULL);

  for (size_t i = 0; i != pending_thumbnail_tasks_.size(); ++i)
    pending_thumbnail_tasks_[i].Run(false);

  // This call is usually no-op since |this| object is already removed from the
  // Aura root window and we don't have a way to get an input method object
  // associated with the window, but just in case.
  DetachFromInputMethod();
}

void RenderWidgetHostViewAura::UpdateCursorIfOverSelf() {
  const gfx::Point screen_point = gfx::Screen::GetCursorScreenPoint();
  aura::RootWindow* root_window = window_->GetRootWindow();
  if (!root_window)
    return;

  if (root_window->GetEventHandlerForPoint(screen_point) != window_)
    return;

  gfx::NativeCursor cursor = current_cursor_.GetNativeCursor();
  if (is_loading_)
    cursor = ui::kCursorPointer;

  aura::client::CursorClient* cursor_client =
      aura::client::GetCursorClient(root_window);
  if (cursor_client)
    cursor_client->SetCursor(cursor);
}

ui::InputMethod* RenderWidgetHostViewAura::GetInputMethod() const {
  aura::RootWindow* root_window = window_->GetRootWindow();
  if (!root_window)
    return NULL;
  return root_window->GetProperty(aura::client::kRootWindowInputMethodKey);
}

bool RenderWidgetHostViewAura::NeedsInputGrab() {
  return popup_type_ == WebKit::WebPopupTypeSelect;
}

void RenderWidgetHostViewAura::FinishImeCompositionSession() {
  if (!has_composition_text_)
    return;
  if (host_)
    host_->ImeConfirmComposition();
  ImeCancelComposition();
}

void RenderWidgetHostViewAura::ModifyEventMovementAndCoords(
    WebKit::WebMouseEvent* event) {
  // If the mouse has just entered, we must report zero movementX/Y. Hence we
  // reset any global_mouse_position set previously.
  if (event->type == WebKit::WebInputEvent::MouseEnter ||
      event->type == WebKit::WebInputEvent::MouseLeave)
    global_mouse_position_.SetPoint(event->globalX, event->globalY);

  // Movement is computed by taking the difference of the new cursor position
  // and the previous. Under mouse lock the cursor will be warped back to the
  // center so that we are not limited by clipping boundaries.
  // We do not measure movement as the delta from cursor to center because
  // we may receive more mouse movement events before our warp has taken
  // effect.
  event->movementX = event->globalX - global_mouse_position_.x();
  event->movementY = event->globalY - global_mouse_position_.y();

  global_mouse_position_.SetPoint(event->globalX, event->globalY);

  // Under mouse lock, coordinates of mouse are locked to what they were when
  // mouse lock was entered.
  if (mouse_locked_) {
    event->x = unlocked_mouse_position_.x();
    event->y = unlocked_mouse_position_.y();
    event->windowX = unlocked_mouse_position_.x();
    event->windowY = unlocked_mouse_position_.y();
    event->globalX = unlocked_global_mouse_position_.x();
    event->globalY = unlocked_global_mouse_position_.y();
  } else {
    unlocked_mouse_position_.SetPoint(event->windowX, event->windowY);
    unlocked_global_mouse_position_.SetPoint(event->globalX, event->globalY);
  }
}

void RenderWidgetHostViewAura::SchedulePaintIfNotInClip(
    const gfx::Rect& rect,
    const gfx::Rect& clip) {
  if (!clip.IsEmpty()) {
    gfx::Rect to_paint = rect.Subtract(clip);
    if (!to_paint.IsEmpty())
      window_->SchedulePaintInRect(to_paint);
  } else {
    window_->SchedulePaintInRect(rect);
  }
}

bool RenderWidgetHostViewAura::ShouldMoveToCenter() {
  gfx::Rect rect = window_->bounds();
  int border_x = rect.width() * kMouseLockBorderPercentage / 100;
  int border_y = rect.height() * kMouseLockBorderPercentage / 100;

  return global_mouse_position_.x() < rect.x() + border_x ||
      global_mouse_position_.x() > rect.right() - border_x ||
      global_mouse_position_.y() < rect.y() + border_y ||
      global_mouse_position_.y() > rect.bottom() - border_y;
}

void RenderWidgetHostViewAura::RunCompositingDidCommitCallbacks(
    ui::Compositor* compositor) {
  for (std::vector< base::Callback<void(ui::Compositor*)> >::const_iterator
      it = on_compositing_did_commit_callbacks_.begin();
      it != on_compositing_did_commit_callbacks_.end(); ++it) {
    it->Run(compositor);
  }
  on_compositing_did_commit_callbacks_.clear();
}

void RenderWidgetHostViewAura::RunCompositingWillStartCallbacks(
    ui::Compositor* compositor) {
  for (std::vector< base::Callback<void(ui::Compositor*)> >::const_iterator
      it = on_compositing_will_start_callbacks_.begin();
      it != on_compositing_will_start_callbacks_.end(); ++it) {
    it->Run(compositor);
  }
  on_compositing_will_start_callbacks_.clear();
}

// static
void RenderWidgetHostViewAura::InsertSyncPointAndACK(
     int32 route_id, int gpu_host_id, ui::Compositor* compositor) {
  uint32 sync_point = 0;
  // If we have no compositor, so we must still send the ACK. A zero
  // sync point will not be waited for in the GPU process.
  if (compositor) {
    ImageTransportFactory* factory = ImageTransportFactory::GetInstance();
    sync_point = factory->InsertSyncPoint();
  }

  RenderWidgetHostImpl::AcknowledgeBufferPresent(
      route_id, gpu_host_id, sync_point);
}

void RenderWidgetHostViewAura::RemovingFromRootWindow() {
  // We are about to disconnect ourselves from the compositor, we need to issue
  // the callbacks now, because we won't get notified when the frame is done.
  // TODO(piman): this might in theory cause a race where the GPU process starts
  // drawing to the buffer we haven't yet displayed. This will only show for 1
  // frame though, because we will reissue a new frame right away without that
  // composited data.
  ui::Compositor* compositor = GetCompositor();
  RunCompositingDidCommitCallbacks(compositor);
  RunCompositingWillStartCallbacks(compositor);
  locks_pending_draw_.clear();
  if (compositor && compositor->HasObserver(this))
    compositor->RemoveObserver(this);
  DetachFromInputMethod();
}

ui::Compositor* RenderWidgetHostViewAura::GetCompositor() {
  aura::RootWindow* root_window = window_->GetRootWindow();
  return root_window ? root_window->compositor() : NULL;
}

void RenderWidgetHostViewAura::DetachFromInputMethod() {
  ui::InputMethod* input_method = GetInputMethod();
  if (input_method && input_method->GetTextInputClient() == this)
    input_method->SetFocusedTextInputClient(NULL);
}

////////////////////////////////////////////////////////////////////////////////
// RenderWidgetHostView, public:

// static
RenderWidgetHostView* RenderWidgetHostView::CreateViewForWidget(
    RenderWidgetHost* widget) {
  return new RenderWidgetHostViewAura(widget);
}

// static
void RenderWidgetHostViewPort::GetDefaultScreenInfo(WebScreenInfo* results) {
  GetScreenInfoForWindow(results, NULL);
}

}  // namespace content
