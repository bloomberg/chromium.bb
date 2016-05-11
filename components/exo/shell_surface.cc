// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/shell_surface.h"

#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/wm/aura/wm_window_aura.h"
#include "ash/wm/common/window_resizer.h"
#include "ash/wm/common/window_state.h"
#include "ash/wm/window_state_aura.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_event_argument.h"
#include "components/exo/surface.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_property.h"
#include "ui/aura/window_targeter.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/hit_test.h"
#include "ui/gfx/path.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/window_util.h"
#include "ui/wm/public/activation_client.h"

DECLARE_WINDOW_PROPERTY_TYPE(std::string*)

namespace exo {
namespace {

class CustomFrameView : public views::NonClientFrameView {
 public:
  explicit CustomFrameView(views::Widget* widget) : widget_(widget) {}
  ~CustomFrameView() override {}

  // Overridden from views::NonClientFrameView:
  gfx::Rect GetBoundsForClientView() const override { return bounds(); }
  gfx::Rect GetWindowBoundsForClientBounds(
      const gfx::Rect& client_bounds) const override {
    return client_bounds;
  }
  int NonClientHitTest(const gfx::Point& point) override {
    return widget_->client_view()->NonClientHitTest(point);
  }
  void GetWindowMask(const gfx::Size& size, gfx::Path* window_mask) override {}
  void ResetWindowControls() override {}
  void UpdateWindowIcon() override {}
  void UpdateWindowTitle() override {}
  void SizeConstraintsChanged() override {}

 private:
  views::Widget* const widget_;

  DISALLOW_COPY_AND_ASSIGN(CustomFrameView);
};

class CustomWindowTargeter : public aura::WindowTargeter {
 public:
  CustomWindowTargeter() {}
  ~CustomWindowTargeter() override {}

  // Overridden from aura::WindowTargeter:
  bool EventLocationInsideBounds(aura::Window* window,
                                 const ui::LocatedEvent& event) const override {
    Surface* surface = ShellSurface::GetMainSurface(window);
    if (!surface)
      return false;

    gfx::Point local_point = event.location();
    if (window->parent())
      aura::Window::ConvertPointToTarget(window->parent(), window,
                                         &local_point);

    aura::Window::ConvertPointToTarget(window, surface, &local_point);
    return surface->HitTestRect(gfx::Rect(local_point, gfx::Size(1, 1)));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(CustomWindowTargeter);
};

class ShellSurfaceWidget : public views::Widget {
 public:
  explicit ShellSurfaceWidget(ShellSurface* shell_surface)
      : shell_surface_(shell_surface) {}

  // Overridden from views::Widget
  void Close() override { shell_surface_->Close(); }
  void OnKeyEvent(ui::KeyEvent* event) override {
    // Handle only accelerators. Do not call Widget::OnKeyEvent that eats focus
    // management keys (like the tab key) as well.
    if (GetFocusManager()->ProcessAccelerator(ui::Accelerator(*event)))
      event->StopPropagation();
  }

 private:
  ShellSurface* const shell_surface_;

  DISALLOW_COPY_AND_ASSIGN(ShellSurfaceWidget);
};

}  // namespace

// Helper class used to coalesce a number of changes into one "configure"
// callback. Callbacks are suppressed while an instance of this class is
// instantiated and instead called when the instance is destroyed.
// If |force_configure_| is true ShellSurface::Configure() will be called
// even if no changes to shell surface took place during the lifetime of the
// ScopedConfigure instance.
class ShellSurface::ScopedConfigure {
 public:
  ScopedConfigure(ShellSurface* shell_surface, bool force_configure);
  ~ScopedConfigure();

  void set_needs_configure() { needs_configure_ = true; }

 private:
  ShellSurface* const shell_surface_;
  const bool force_configure_;
  bool needs_configure_;

  DISALLOW_COPY_AND_ASSIGN(ScopedConfigure);
};

////////////////////////////////////////////////////////////////////////////////
// ShellSurface, ScopedConfigure:

ShellSurface::ScopedConfigure::ScopedConfigure(ShellSurface* shell_surface,
                                               bool force_configure)
    : shell_surface_(shell_surface),
      force_configure_(force_configure),
      needs_configure_(false) {
  // ScopedConfigure instances cannot be nested.
  DCHECK(!shell_surface_->scoped_configure_);
  shell_surface_->scoped_configure_ = this;
}

ShellSurface::ScopedConfigure::~ScopedConfigure() {
  DCHECK_EQ(shell_surface_->scoped_configure_, this);
  shell_surface_->scoped_configure_ = nullptr;
  if (needs_configure_ || force_configure_)
    shell_surface_->Configure();
  // ScopedConfigure instance might have suppressed a widget bounds update.
  if (shell_surface_->widget_)
    shell_surface_->UpdateWidgetBounds();
}

////////////////////////////////////////////////////////////////////////////////
// ShellSurface, public:

DEFINE_LOCAL_WINDOW_PROPERTY_KEY(std::string*, kApplicationIdKey, nullptr)
DEFINE_LOCAL_WINDOW_PROPERTY_KEY(Surface*, kMainSurfaceKey, nullptr)

ShellSurface::ShellSurface(Surface* surface,
                           ShellSurface* parent,
                           const gfx::Rect& initial_bounds,
                           bool activatable)
    : widget_(nullptr),
      surface_(surface),
      parent_(parent ? parent->GetWidget()->GetNativeWindow() : nullptr),
      initial_bounds_(initial_bounds),
      activatable_(activatable),
      scoped_configure_(nullptr),
      ignore_window_bounds_changes_(false),
      resize_component_(HTCAPTION),
      pending_resize_component_(HTCAPTION) {
  ash::Shell::GetInstance()->activation_client()->AddObserver(this);
  surface_->SetSurfaceDelegate(this);
  surface_->AddSurfaceObserver(this);
  surface_->Show();
  set_owned_by_client();
  if (parent_)
    parent_->AddObserver(this);
}

ShellSurface::ShellSurface(Surface* surface)
    : ShellSurface(surface, nullptr, gfx::Rect(), true) {}

ShellSurface::~ShellSurface() {
  DCHECK(!scoped_configure_);
  ash::Shell::GetInstance()->activation_client()->RemoveObserver(this);
  if (surface_) {
    surface_->SetSurfaceDelegate(nullptr);
    surface_->RemoveSurfaceObserver(this);
  }
  if (parent_)
    parent_->RemoveObserver(this);
  if (resizer_)
    EndDrag(false /* revert */);
  if (widget_) {
    ash::wm::GetWindowState(widget_->GetNativeWindow())->RemoveObserver(this);
    widget_->GetNativeWindow()->RemoveObserver(this);
    if (widget_->IsVisible())
      widget_->Hide();
    widget_->CloseNow();
  }
}

void ShellSurface::AcknowledgeConfigure(uint32_t serial) {
  TRACE_EVENT1("exo", "ShellSurface::AcknowledgeConfigure", "serial", serial);

  // Apply all configs that are older or equal to |serial|. The result is that
  // the origin of the main surface will move and the resize direction will
  // change to reflect the acknowledgement of configure request with |serial|
  // at the next call to Commit().
  while (!pending_configs_.empty()) {
    auto config = pending_configs_.front();
    pending_configs_.pop_front();

    // Add the config offset to the accumulated offset that will be applied when
    // Commit() is called.
    pending_origin_offset_ += config.origin_offset;

    // Set the resize direction that will be applied when Commit() is called.
    pending_resize_component_ = config.resize_component;

    if (config.serial == serial)
      break;
  }

  if (widget_)
    UpdateWidgetBounds();
}

void ShellSurface::SetParent(ShellSurface* parent) {
  TRACE_EVENT1("exo", "ShellSurface::SetParent", "parent",
               parent ? base::UTF16ToASCII(parent->title_) : "null");

  if (parent_) {
    parent_->RemoveObserver(this);
    if (widget_)
      wm::RemoveTransientChild(parent_, widget_->GetNativeWindow());
  }
  parent_ = parent ? parent->GetWidget()->GetNativeWindow() : nullptr;
  if (parent_) {
    parent_->AddObserver(this);
    if (widget_)
      wm::AddTransientChild(parent_, widget_->GetNativeWindow());
  }
}

void ShellSurface::Maximize() {
  TRACE_EVENT0("exo", "ShellSurface::Maximize");

  if (!widget_)
    CreateShellSurfaceWidget(ui::SHOW_STATE_MAXIMIZED);

  // Note: This will ask client to configure its surface even if already
  // maximized.
  ScopedConfigure scoped_configure(this, true);
  widget_->Maximize();
}

void ShellSurface::Restore() {
  TRACE_EVENT0("exo", "ShellSurface::Restore");

  if (!widget_)
    return;

  // Note: This will ask client to configure its surface even if not already
  // maximized.
  ScopedConfigure scoped_configure(this, true);
  widget_->Restore();
}

void ShellSurface::SetFullscreen(bool fullscreen) {
  TRACE_EVENT1("exo", "ShellSurface::SetFullscreen", "fullscreen", fullscreen);

  if (!widget_)
    CreateShellSurfaceWidget(ui::SHOW_STATE_FULLSCREEN);

  // Note: This will ask client to configure its surface even if fullscreen
  // state doesn't change.
  ScopedConfigure scoped_configure(this, true);
  widget_->SetFullscreen(fullscreen);
}

void ShellSurface::SetTitle(const base::string16& title) {
  TRACE_EVENT1("exo", "ShellSurface::SetTitle", "title",
               base::UTF16ToUTF8(title));

  title_ = title;
  if (widget_)
    widget_->UpdateWindowTitle();
}

// static
void ShellSurface::SetApplicationId(aura::Window* window,
                                    std::string* application_id) {
  window->SetProperty(kApplicationIdKey, application_id);
}

// static
const std::string ShellSurface::GetApplicationId(aura::Window* window) {
  std::string* string_ptr = window->GetProperty(kApplicationIdKey);
  return string_ptr ? *string_ptr : std::string();
}

void ShellSurface::SetApplicationId(const std::string& application_id) {
  TRACE_EVENT1("exo", "ShellSurface::SetApplicationId", "application_id",
               application_id);

  application_id_ = application_id;
}

void ShellSurface::Move() {
  TRACE_EVENT0("exo", "ShellSurface::Move");

  if (widget_)
    AttemptToStartDrag(HTCAPTION);
}

void ShellSurface::Resize(int component) {
  TRACE_EVENT1("exo", "ShellSurface::Resize", "component", component);

  if (widget_)
    AttemptToStartDrag(component);
}

void ShellSurface::Close() {
  if (!close_callback_.is_null())
    close_callback_.Run();
}

void ShellSurface::SetGeometry(const gfx::Rect& geometry) {
  TRACE_EVENT1("exo", "ShellSurface::SetGeometry", "geometry",
               geometry.ToString());

  if (geometry.IsEmpty()) {
    DLOG(WARNING) << "Surface geometry must be non-empty";
    return;
  }

  pending_geometry_ = geometry;
}

// static
void ShellSurface::SetMainSurface(aura::Window* window, Surface* surface) {
  window->SetProperty(kMainSurfaceKey, surface);
}

// static
Surface* ShellSurface::GetMainSurface(const aura::Window* window) {
  return window->GetProperty(kMainSurfaceKey);
}

std::unique_ptr<base::trace_event::TracedValue> ShellSurface::AsTracedValue()
    const {
  std::unique_ptr<base::trace_event::TracedValue> value(
      new base::trace_event::TracedValue());
  value->SetString("title", base::UTF16ToUTF8(title_));
  value->SetString("application_id", application_id_);
  return value;
}

////////////////////////////////////////////////////////////////////////////////
// SurfaceDelegate overrides:

void ShellSurface::OnSurfaceCommit() {
  surface_->CommitSurfaceHierarchy();

  // Apply new window geometry.
  geometry_ = pending_geometry_;

  if (enabled() && !widget_)
    CreateShellSurfaceWidget(ui::SHOW_STATE_NORMAL);

  // Apply the accumulated pending origin offset to reflect acknowledged
  // configure requests.
  origin_ += pending_origin_offset_;
  pending_origin_offset_ = gfx::Vector2d();

  // Update resize direction to reflect acknowledged configure requests.
  resize_component_ = pending_resize_component_;

  if (widget_) {
    UpdateWidgetBounds();

    gfx::Point surface_origin = GetSurfaceOrigin();
    gfx::Rect hit_test_bounds =
        surface_->GetHitTestBounds() + surface_origin.OffsetFromOrigin();

    // Prevent window from being activated when hit test bounds are empty.
    bool activatable = activatable_ && !hit_test_bounds.IsEmpty();
    if (activatable != CanActivate()) {
      set_can_activate(activatable);

      // Activate or deactivate window if activation state changed.
      aura::client::ActivationClient* activation_client =
          ash::Shell::GetInstance()->activation_client();
      if (activatable)
        activation_client->ActivateWindow(widget_->GetNativeWindow());
      else if (widget_->IsActive())
        activation_client->DeactivateWindow(widget_->GetNativeWindow());
    }

    UpdateTransparentInsets();

    // Update surface bounds.
    surface_->SetBounds(gfx::Rect(surface_origin, surface_->layer()->size()));

    // Show widget if not already visible.
    if (!widget_->IsClosed() && !widget_->IsVisible())
      widget_->Show();
  }
}

bool ShellSurface::IsSurfaceSynchronized() const {
  // A shell surface is always desynchronized.
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// SurfaceObserver overrides:

void ShellSurface::OnSurfaceDestroying(Surface* surface) {
  if (resizer_)
    EndDrag(false /* revert */);
  if (widget_)
    SetMainSurface(widget_->GetNativeWindow(), nullptr);
  surface->RemoveSurfaceObserver(this);
  surface_ = nullptr;

  // Hide widget before surface is destroyed. This allows hide animations to
  // run using the current surface contents.
  if (widget_)
    widget_->Hide();

  // Note: In its use in the Wayland server implementation, the surface
  // destroyed callback may destroy the ShellSurface instance. This call needs
  // to be last so that the instance can be destroyed.
  if (!surface_destroyed_callback_.is_null())
    surface_destroyed_callback_.Run();
}

////////////////////////////////////////////////////////////////////////////////
// views::WidgetDelegate overrides:

bool ShellSurface::CanMaximize() const {
  return true;
}

bool ShellSurface::CanResize() const {
  return true;
}

base::string16 ShellSurface::GetWindowTitle() const {
  return title_;
}

void ShellSurface::WindowClosing() {
  if (resizer_)
    EndDrag(true /* revert */);
  SetEnabled(false);
  widget_ = nullptr;
}

views::Widget* ShellSurface::GetWidget() {
  return widget_;
}

const views::Widget* ShellSurface::GetWidget() const {
  return widget_;
}

views::View* ShellSurface::GetContentsView() {
  return this;
}

views::NonClientFrameView* ShellSurface::CreateNonClientFrameView(
    views::Widget* widget) {
  return new CustomFrameView(widget);
}

bool ShellSurface::WidgetHasHitTestMask() const {
  return surface_ ? surface_->HasHitTestMask() : false;
}

void ShellSurface::GetWidgetHitTestMask(gfx::Path* mask) const {
  DCHECK(WidgetHasHitTestMask());
  surface_->GetHitTestMask(mask);
  gfx::Point origin = surface_->bounds().origin();
  mask->offset(SkIntToScalar(origin.x()), SkIntToScalar(origin.y()));
}

////////////////////////////////////////////////////////////////////////////////
// views::Views overrides:

gfx::Size ShellSurface::GetPreferredSize() const {
  if (!geometry_.IsEmpty())
    return geometry_.size();

  return surface_ ? surface_->layer()->size() : gfx::Size();
}

////////////////////////////////////////////////////////////////////////////////
// ash::wm::WindowStateObserver overrides:

void ShellSurface::OnPostWindowStateTypeChange(
    ash::wm::WindowState* window_state,
    ash::wm::WindowStateType old_type) {
  ash::wm::WindowStateType new_type = window_state->GetStateType();
  if (old_type == ash::wm::WINDOW_STATE_TYPE_MAXIMIZED ||
      new_type == ash::wm::WINDOW_STATE_TYPE_MAXIMIZED ||
      old_type == ash::wm::WINDOW_STATE_TYPE_FULLSCREEN ||
      new_type == ash::wm::WINDOW_STATE_TYPE_FULLSCREEN) {
    Configure();
  }

  if (widget_)
    UpdateWidgetBounds();
}

////////////////////////////////////////////////////////////////////////////////
// aura::WindowObserver overrides:

void ShellSurface::OnWindowBoundsChanged(aura::Window* window,
                                         const gfx::Rect& old_bounds,
                                         const gfx::Rect& new_bounds) {
  if (!widget_ || !surface_ || ignore_window_bounds_changes_)
    return;

  if (window == widget_->GetNativeWindow()) {
    if (new_bounds.size() == old_bounds.size())
      return;

    // If size changed then give the client a chance to produce new contents
    // before origin on screen is changed by adding offset to the next configure
    // request and offset |origin_| by the same distance.
    gfx::Vector2d origin_offset = new_bounds.origin() - old_bounds.origin();
    pending_origin_config_offset_ += origin_offset;
    origin_ -= origin_offset;

    UpdateTransparentInsets();

    surface_->SetBounds(
        gfx::Rect(GetSurfaceOrigin(), surface_->layer()->size()));

    Configure();
  }
}

void ShellSurface::OnWindowDestroying(aura::Window* window) {
  if (window == parent_) {
    parent_ = nullptr;
    // Disable shell surface in case parent is destroyed before shell surface
    // widget has been created.
    SetEnabled(false);
  }
  window->RemoveObserver(this);
}

////////////////////////////////////////////////////////////////////////////////
// aura::client::ActivationChangeObserver overrides:

void ShellSurface::OnWindowActivated(
    aura::client::ActivationChangeObserver::ActivationReason reason,
    aura::Window* gained_active,
    aura::Window* lost_active) {
  if (!widget_)
    return;

  if (gained_active == widget_->GetNativeWindow() ||
      lost_active == widget_->GetNativeWindow()) {
    DCHECK(activatable_);
    Configure();
  }
}

////////////////////////////////////////////////////////////////////////////////
// ui::EventHandler overrides:

void ShellSurface::OnKeyEvent(ui::KeyEvent* event) {
  if (!resizer_) {
    views::View::OnKeyEvent(event);
    return;
  }

  if (event->type() == ui::ET_KEY_PRESSED &&
      event->key_code() == ui::VKEY_ESCAPE) {
    EndDrag(true /* revert */);
  }
}

void ShellSurface::OnMouseEvent(ui::MouseEvent* event) {
  if (!resizer_) {
    views::View::OnMouseEvent(event);
    return;
  }

  if (event->handled())
    return;

  if ((event->flags() &
       (ui::EF_MIDDLE_MOUSE_BUTTON | ui::EF_RIGHT_MOUSE_BUTTON)) != 0)
    return;

  if (event->type() == ui::ET_MOUSE_CAPTURE_CHANGED) {
    // We complete the drag instead of reverting it, as reverting it will
    // result in a weird behavior when a client produces a modal dialog
    // while the drag is in progress.
    EndDrag(false /* revert */);
    return;
  }

  switch (event->type()) {
    case ui::ET_MOUSE_DRAGGED: {
      gfx::Point location(event->location());
      aura::Window::ConvertPointToTarget(widget_->GetNativeWindow(),
                                         widget_->GetNativeWindow()->parent(),
                                         &location);
      ScopedConfigure scoped_configure(this, false);
      resizer_->Drag(location, event->flags());
      event->StopPropagation();
      break;
    }
    case ui::ET_MOUSE_RELEASED: {
      ScopedConfigure scoped_configure(this, false);
      EndDrag(false /* revert */);
      break;
    }
    case ui::ET_MOUSE_MOVED:
    case ui::ET_MOUSE_PRESSED:
    case ui::ET_MOUSE_ENTERED:
    case ui::ET_MOUSE_EXITED:
    case ui::ET_MOUSEWHEEL:
    case ui::ET_MOUSE_CAPTURE_CHANGED:
      break;
    default:
      NOTREACHED();
      break;
  }
}

////////////////////////////////////////////////////////////////////////////////
// ShellSurface, private:

void ShellSurface::CreateShellSurfaceWidget(ui::WindowShowState show_state) {
  DCHECK(enabled());
  DCHECK(!widget_);

  views::Widget::InitParams params;
  params.type = views::Widget::InitParams::TYPE_WINDOW;
  params.ownership = views::Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET;
  params.delegate = this;
  params.shadow_type = views::Widget::InitParams::SHADOW_TYPE_NONE;
  params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
  params.show_state = show_state;
  params.parent = ash::Shell::GetContainer(
      ash::Shell::GetPrimaryRootWindow(), ash::kShellWindowId_DefaultContainer);
  if (!initial_bounds_.IsEmpty()) {
    params.bounds = initial_bounds_;
    if (parent_) {
      aura::Window::ConvertRectToTarget(GetMainSurface(parent_), params.parent,
                                        &params.bounds);
    }
  }
  bool activatable = activatable_ && !surface_->GetHitTestBounds().IsEmpty();
  params.activatable = activatable ? views::Widget::InitParams::ACTIVATABLE_YES
                                   : views::Widget::InitParams::ACTIVATABLE_NO;

  // Note: NativeWidget owns this widget.
  widget_ = new ShellSurfaceWidget(this);
  widget_->Init(params);

  aura::Window* window = widget_->GetNativeWindow();
  window->SetName("ExoShellSurface");
  window->AddChild(surface_);
  window->SetEventTargeter(base::WrapUnique(new CustomWindowTargeter));
  SetApplicationId(window, &application_id_);
  SetMainSurface(window, surface_);

  // Start tracking changes to window bounds and window state.
  window->AddObserver(this);
  ash::wm::GetWindowState(window)->AddObserver(this);

  // Make shell surface a transient child if |parent_| has been set.
  if (parent_)
    wm::AddTransientChild(parent_, window);

  // Allow Ash to manage the position of a top-level shell surfaces if show
  // state is one that allows auto positioning and |initial_bounds_| has
  // not been set.
  ash::wm::GetWindowState(window)->set_window_position_managed(
      ash::wm::ToWindowShowState(ash::wm::WINDOW_STATE_TYPE_AUTO_POSITIONED) ==
          show_state &&
      initial_bounds_.IsEmpty());

  // The transparent insets cover the whole window until we have some initial
  // contents.
  ash::wm::GetWindowState(window)->set_transparent_insets(gfx::Insets(
      window->bounds().size().width(), window->bounds().size().height()));
}

void ShellSurface::Configure() {
  DCHECK(widget_);

  // Delay configure callback if |scoped_configure_| is set.
  if (scoped_configure_) {
    scoped_configure_->set_needs_configure();
    return;
  }

  gfx::Vector2d origin_offset = pending_origin_config_offset_;
  pending_origin_config_offset_ = gfx::Vector2d();

  // If surface is being resized, save the resize direction.
  int resize_component =
      resizer_ ? resizer_->details().window_component : HTCAPTION;

  if (configure_callback_.is_null()) {
    pending_origin_offset_ += origin_offset;
    pending_resize_component_ = resize_component;
    return;
  }

  uint32_t serial = configure_callback_.Run(
      widget_->GetWindowBoundsInScreen().size(),
      ash::wm::GetWindowState(widget_->GetNativeWindow())->GetStateType(),
      IsResizing(), widget_->IsActive());

  // Apply origin offset and resize component at the first Commit() after this
  // configure request has been acknowledged.
  pending_configs_.push_back({serial, origin_offset, resize_component});
  LOG_IF(WARNING, pending_configs_.size() > 100)
      << "Number of pending configure acks for shell surface has reached: "
      << pending_configs_.size();
}

void ShellSurface::AttemptToStartDrag(int component) {
  DCHECK(widget_);

  // Cannot start another drag if one is already taking place.
  if (resizer_)
    return;

  if (widget_->GetNativeWindow()->HasCapture())
    return;

  aura::Window* root_window = widget_->GetNativeWindow()->GetRootWindow();
  gfx::Point drag_location =
      root_window->GetHost()->dispatcher()->GetLastMouseLocationInRoot();
  aura::Window::ConvertPointToTarget(
      root_window, widget_->GetNativeWindow()->parent(), &drag_location);

  // Set the cursor before calling CreateWindowResizer(), as that will
  // eventually call LockCursor() and prevent the cursor from changing.
  aura::client::CursorClient* cursor_client =
      aura::client::GetCursorClient(root_window);
  DCHECK(cursor_client);

  switch (component) {
    case HTCAPTION:
      cursor_client->SetCursor(ui::kCursorPointer);
      break;
    case HTTOP:
      cursor_client->SetCursor(ui::kCursorNorthResize);
      break;
    case HTTOPRIGHT:
      cursor_client->SetCursor(ui::kCursorNorthEastResize);
      break;
    case HTRIGHT:
      cursor_client->SetCursor(ui::kCursorEastResize);
      break;
    case HTBOTTOMRIGHT:
      cursor_client->SetCursor(ui::kCursorSouthEastResize);
      break;
    case HTBOTTOM:
      cursor_client->SetCursor(ui::kCursorSouthResize);
      break;
    case HTBOTTOMLEFT:
      cursor_client->SetCursor(ui::kCursorSouthWestResize);
      break;
    case HTLEFT:
      cursor_client->SetCursor(ui::kCursorWestResize);
      break;
    case HTTOPLEFT:
      cursor_client->SetCursor(ui::kCursorNorthWestResize);
      break;
    default:
      NOTREACHED();
      break;
  }

  resizer_ = ash::CreateWindowResizer(
      ash::wm::WmWindowAura::Get(widget_->GetNativeWindow()), drag_location,
      component, aura::client::WINDOW_MOVE_SOURCE_MOUSE);
  if (!resizer_)
    return;

  // Apply pending origin offsets and resize direction before starting a new
  // resize operation. These can still be pending if the client has acknowledged
  // the configure request but not yet called Commit().
  origin_ += pending_origin_offset_;
  pending_origin_offset_ = gfx::Vector2d();
  resize_component_ = pending_resize_component_;

  ash::Shell::GetInstance()->AddPreTargetHandler(this);
  widget_->GetNativeWindow()->SetCapture();

  // Notify client that resizing state has changed.
  if (IsResizing())
    Configure();
}

void ShellSurface::EndDrag(bool revert) {
  DCHECK(widget_);
  DCHECK(resizer_);

  bool was_resizing = IsResizing();

  if (revert)
    resizer_->RevertDrag();
  else
    resizer_->CompleteDrag();

  ash::Shell::GetInstance()->RemovePreTargetHandler(this);
  widget_->GetNativeWindow()->ReleaseCapture();
  resizer_.reset();

  // Notify client that resizing state has changed.
  if (was_resizing)
    Configure();

  UpdateWidgetBounds();
}

bool ShellSurface::IsResizing() const {
  if (!resizer_)
    return false;

  return resizer_->details().bounds_change &
         ash::WindowResizer::kBoundsChange_Resizes;
}

gfx::Rect ShellSurface::GetVisibleBounds() const {
  // Use |geometry_| if set, otherwise use the visual bounds of the surface.
  return geometry_.IsEmpty() ? gfx::Rect(surface_->layer()->size()) : geometry_;
}

gfx::Point ShellSurface::GetSurfaceOrigin() const {
  gfx::Rect window_bounds = widget_->GetWindowBoundsInScreen();
  gfx::Rect visible_bounds = GetVisibleBounds();

  switch (resize_component_) {
    case HTCAPTION:
      return origin_ - visible_bounds.OffsetFromOrigin();
    case HTBOTTOM:
    case HTRIGHT:
    case HTBOTTOMRIGHT:
      return gfx::Point() - visible_bounds.OffsetFromOrigin();
    case HTTOP:
    case HTTOPRIGHT:
      return gfx::Point(0, window_bounds.height() - visible_bounds.height()) -
             visible_bounds.OffsetFromOrigin();
      break;
    case HTLEFT:
    case HTBOTTOMLEFT:
      return gfx::Point(window_bounds.width() - visible_bounds.width(), 0) -
             visible_bounds.OffsetFromOrigin();
    case HTTOPLEFT:
      return gfx::Point(window_bounds.width() - visible_bounds.width(),
                        window_bounds.height() - visible_bounds.height()) -
             visible_bounds.OffsetFromOrigin();
    default:
      NOTREACHED();
      return gfx::Point();
  }
}

void ShellSurface::UpdateWidgetBounds() {
  DCHECK(widget_);

  // Return early if the shell is currently managing the bounds of the widget.
  if (widget_->IsMaximized() || widget_->IsFullscreen() || IsResizing())
    return;

  // Return early if there is pending configure requests.
  if (!pending_configs_.empty() || scoped_configure_)
    return;

  gfx::Rect visible_bounds = GetVisibleBounds();
  gfx::Rect new_widget_bounds(widget_->GetNativeWindow()->bounds().origin(),
                              visible_bounds.size());

  // Update widget origin using the surface origin if the current location of
  // surface is being anchored to one side of the widget as a result of a
  // resize operation.
  if (resize_component_ != HTCAPTION) {
    gfx::Point new_widget_origin =
        GetSurfaceOrigin() + visible_bounds.OffsetFromOrigin();
    aura::Window::ConvertPointToTarget(widget_->GetNativeWindow(),
                                       widget_->GetNativeWindow()->parent(),
                                       &new_widget_origin);
    new_widget_bounds.set_origin(new_widget_origin);
  }

  // Set |ignore_window_bounds_changes_| as this change to window bounds
  // should not result in a configure request.
  DCHECK(!ignore_window_bounds_changes_);
  ignore_window_bounds_changes_ = true;
  widget_->SetBounds(new_widget_bounds);
  ignore_window_bounds_changes_ = false;

  UpdateTransparentInsets();

  // A change to the widget size requires surface bounds to be re-adjusted.
  surface_->SetBounds(gfx::Rect(GetSurfaceOrigin(), surface_->layer()->size()));
}

void ShellSurface::UpdateTransparentInsets() {
  DCHECK(widget_);

  gfx::Rect hit_test_bounds =
      surface_->GetHitTestBounds() + GetSurfaceOrigin().OffsetFromOrigin();
  gfx::Size window_size = widget_->GetNativeWindow()->bounds().size();
  gfx::Insets transparent_insets =
      gfx::Rect(window_size).InsetsFrom(hit_test_bounds);
  ash::wm::WindowState* window_state =
      ash::wm::GetWindowState(widget_->GetNativeWindow());
  if (window_state->transparent_insets() != transparent_insets) {
    window_state->set_transparent_insets(transparent_insets);
    ash::Shell::GetInstance()->UpdateShelfVisibility();
  }
}

}  // namespace exo
