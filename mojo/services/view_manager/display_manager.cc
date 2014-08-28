// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/view_manager/display_manager.h"

#include "base/auto_reset.h"
#include "base/scoped_observer.h"
#include "mojo/public/cpp/application/application_connection.h"
#include "mojo/services/public/interfaces/gpu/gpu.mojom.h"
#include "mojo/services/view_manager/connection_manager.h"
#include "mojo/services/view_manager/display_manager_delegate.h"
#include "mojo/services/view_manager/screen_impl.h"
#include "mojo/services/view_manager/window_tree_host_impl.h"
#include "ui/aura/client/default_capture_client.h"
#include "ui/aura/client/focus_client.h"
#include "ui/aura/client/window_tree_client.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/hit_test.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/native_widget_types.h"

namespace mojo {
namespace service {
namespace {

gfx::Rect ConvertRectToRoot(const ServerView* view, const gfx::Rect& bounds) {
  gfx::Point origin(bounds.origin());
  while (view->parent()) {
    origin += view->bounds().OffsetFromOrigin();
    view = view->parent();
  }
  return gfx::Rect(origin, bounds.size());
}

void PaintViewTree(gfx::Canvas* canvas,
                   const ServerView* view,
                   const gfx::Point& origin) {
  if (!view->visible())
    return;

  canvas->DrawImageInt(gfx::ImageSkia::CreateFrom1xBitmap(view->bitmap()),
                       origin.x(),
                       origin.y());
  std::vector<const ServerView*> children(view->GetChildren());
  for (size_t i = 0; i < children.size(); ++i) {
    PaintViewTree(
        canvas, children[i], origin + children[i]->bounds().OffsetFromOrigin());
  }
}

}  // namespace

class DisplayManager::RootWindowDelegateImpl : public aura::WindowDelegate {
 public:
  explicit RootWindowDelegateImpl(const ServerView* root_view)
      : root_view_(root_view) {}
  virtual ~RootWindowDelegateImpl() {}

  // aura::WindowDelegate:
  virtual gfx::Size GetMinimumSize() const OVERRIDE {
    return gfx::Size();
  }
  virtual gfx::Size GetMaximumSize() const OVERRIDE {
    return gfx::Size();
  }
  virtual void OnBoundsChanged(const gfx::Rect& old_bounds,
                               const gfx::Rect& new_bounds) OVERRIDE {
  }
  virtual gfx::NativeCursor GetCursor(const gfx::Point& point) OVERRIDE {
    return gfx::kNullCursor;
  }
  virtual int GetNonClientComponent(const gfx::Point& point) const OVERRIDE {
    return HTCAPTION;
  }
  virtual bool ShouldDescendIntoChildForEventHandling(
      aura::Window* child,
      const gfx::Point& location) OVERRIDE {
    return true;
  }
  virtual bool CanFocus() OVERRIDE {
    return true;
  }
  virtual void OnCaptureLost() OVERRIDE {
  }
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE {
    PaintViewTree(canvas, root_view_, gfx::Point());
  }
  virtual void OnDeviceScaleFactorChanged(float device_scale_factor) OVERRIDE {
  }
  virtual void OnWindowDestroying(aura::Window* window) OVERRIDE {
  }
  virtual void OnWindowDestroyed(aura::Window* window) OVERRIDE {
  }
  virtual void OnWindowTargetVisibilityChanged(bool visible) OVERRIDE {
  }
  virtual bool HasHitTestMask() const OVERRIDE {
    return false;
  }
  virtual void GetHitTestMask(gfx::Path* mask) const OVERRIDE {
  }

 private:
  const ServerView* root_view_;

  DISALLOW_COPY_AND_ASSIGN(RootWindowDelegateImpl);
};

// TODO(sky): Remove once aura is removed from the service.
class FocusClientImpl : public aura::client::FocusClient {
 public:
  FocusClientImpl() {}
  virtual ~FocusClientImpl() {}

 private:
  // Overridden from aura::client::FocusClient:
  virtual void AddObserver(
      aura::client::FocusChangeObserver* observer) OVERRIDE {}
  virtual void RemoveObserver(
      aura::client::FocusChangeObserver* observer) OVERRIDE {}
  virtual void FocusWindow(aura::Window* window) OVERRIDE {}
  virtual void ResetFocusWithinActiveWindow(aura::Window* window) OVERRIDE {}
  virtual aura::Window* GetFocusedWindow() OVERRIDE { return NULL; }

  DISALLOW_COPY_AND_ASSIGN(FocusClientImpl);
};

class WindowTreeClientImpl : public aura::client::WindowTreeClient {
 public:
  explicit WindowTreeClientImpl(aura::Window* window) : window_(window) {
    aura::client::SetWindowTreeClient(window_, this);
  }

  virtual ~WindowTreeClientImpl() {
    aura::client::SetWindowTreeClient(window_, NULL);
  }

  // Overridden from aura::client::WindowTreeClient:
  virtual aura::Window* GetDefaultParent(aura::Window* context,
                                         aura::Window* window,
                                         const gfx::Rect& bounds) OVERRIDE {
    if (!capture_client_) {
      capture_client_.reset(
          new aura::client::DefaultCaptureClient(window_->GetRootWindow()));
    }
    return window_;
  }

 private:
  aura::Window* window_;

  scoped_ptr<aura::client::DefaultCaptureClient> capture_client_;

  DISALLOW_COPY_AND_ASSIGN(WindowTreeClientImpl);
};

DisplayManager::DisplayManager(
    ApplicationConnection* app_connection,
    ConnectionManager* connection_manager,
    DisplayManagerDelegate* delegate,
    const Callback<void()>& native_viewport_closed_callback)
    : delegate_(delegate),
      connection_manager_(connection_manager),
      in_setup_(false),
      root_window_(NULL) {
  screen_.reset(ScreenImpl::Create());
  gfx::Screen::SetScreenInstance(gfx::SCREEN_TYPE_NATIVE, screen_.get());
  NativeViewportPtr viewport;
  app_connection->ConnectToService(
      "mojo:mojo_native_viewport_service", &viewport);
  GpuPtr gpu_service;
  // TODO(jamesr): Should be mojo:mojo_gpu_service
  app_connection->ConnectToService("mojo:mojo_native_viewport_service",
                                   &gpu_service);
  window_tree_host_.reset(new WindowTreeHostImpl(
      viewport.Pass(),
      gpu_service.Pass(),
      gfx::Rect(800, 600),
      base::Bind(&DisplayManager::OnCompositorCreated, base::Unretained(this)),
      native_viewport_closed_callback,
      base::Bind(&ConnectionManager::DispatchViewInputEventToWindowManager,
                 base::Unretained(connection_manager_))));
}

DisplayManager::~DisplayManager() {
  window_tree_client_.reset();
  window_tree_host_.reset();
  gfx::Screen::SetScreenInstance(gfx::SCREEN_TYPE_NATIVE, NULL);
}

void DisplayManager::SchedulePaint(const ServerView* view,
                                   const gfx::Rect& bounds) {
  if (root_window_)
    root_window_->SchedulePaintInRect(ConvertRectToRoot(view, bounds));
}

void DisplayManager::OnCompositorCreated() {
  base::AutoReset<bool> resetter(&in_setup_, true);
  window_tree_host_->InitHost();

  window_delegate_.reset(
      new RootWindowDelegateImpl(connection_manager_->root()));
  root_window_ = new aura::Window(window_delegate_.get());
  root_window_->Init(aura::WINDOW_LAYER_TEXTURED);
  root_window_->Show();
  root_window_->SetBounds(
      gfx::Rect(window_tree_host_->window()->bounds().size()));
  window_tree_host_->window()->AddChild(root_window_);

  connection_manager_->root()->SetBounds(
      gfx::Rect(window_tree_host_->window()->bounds().size()));

  window_tree_client_.reset(
      new WindowTreeClientImpl(window_tree_host_->window()));

  focus_client_.reset(new FocusClientImpl);
  aura::client::SetFocusClient(window_tree_host_->window(),
                               focus_client_.get());

  window_tree_host_->Show();

  delegate_->OnDisplayManagerWindowTreeHostCreated();
}

}  // namespace service
}  // namespace mojo
