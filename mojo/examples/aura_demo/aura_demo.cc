// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>
#include <string>

#include "base/bind.h"
#include "mojo/aura/screen_mojo.h"
#include "mojo/examples/aura_demo/context_factory_view_manager.h"
#include "mojo/examples/aura_demo/window_tree_host_view_manager.h"
#include "mojo/public/cpp/application/application.h"
#include "mojo/public/cpp/system/core.h"
#include "mojo/public/interfaces/service_provider/service_provider.mojom.h"
#include "mojo/services/native_viewport/native_viewport.mojom.h"
#include "mojo/services/public/interfaces/view_manager/view_manager.mojom.h"
#include "ui/aura/client/default_capture_client.h"
#include "ui/aura/client/window_tree_client.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/base/hit_test.h"
#include "ui/gfx/canvas.h"

namespace mojo {
namespace examples {

// Trivial WindowDelegate implementation that draws a colored background.
class DemoWindowDelegate : public aura::WindowDelegate {
 public:
  explicit DemoWindowDelegate(SkColor color) : color_(color) {}

  // Overridden from WindowDelegate:
  virtual gfx::Size GetMinimumSize() const OVERRIDE {
    return gfx::Size();
  }

  virtual gfx::Size GetMaximumSize() const OVERRIDE {
    return gfx::Size();
  }

  virtual void OnBoundsChanged(const gfx::Rect& old_bounds,
                               const gfx::Rect& new_bounds) OVERRIDE {}
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
  virtual bool CanFocus() OVERRIDE { return true; }
  virtual void OnCaptureLost() OVERRIDE {}
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE {
    canvas->DrawColor(color_, SkXfermode::kSrc_Mode);
  }
  virtual void OnDeviceScaleFactorChanged(float device_scale_factor) OVERRIDE {}
  virtual void OnWindowDestroying(aura::Window* window) OVERRIDE {}
  virtual void OnWindowDestroyed(aura::Window* window) OVERRIDE {}
  virtual void OnWindowTargetVisibilityChanged(bool visible) OVERRIDE {}
  virtual bool HasHitTestMask() const OVERRIDE { return false; }
  virtual void GetHitTestMask(gfx::Path* mask) const OVERRIDE {}

 private:
  const SkColor color_;

  DISALLOW_COPY_AND_ASSIGN(DemoWindowDelegate);
};

class DemoWindowTreeClient : public aura::client::WindowTreeClient {
 public:
  explicit DemoWindowTreeClient(aura::Window* window) : window_(window) {
    aura::client::SetWindowTreeClient(window_, this);
  }

  virtual ~DemoWindowTreeClient() {
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

  DISALLOW_COPY_AND_ASSIGN(DemoWindowTreeClient);
};

class AuraDemo;

// Trivial IViewManagerClient implementation. Forwards to AuraDemo when
// connection established.
class IViewManagerClientImpl
    : public InterfaceImpl<view_manager::IViewManagerClient> {
 public:
  explicit IViewManagerClientImpl(AuraDemo* aura_demo)
      : aura_demo_(aura_demo) {}
  virtual ~IViewManagerClientImpl() {}

 private:
  void OnResult(bool result) {
    VLOG(1) << "IViewManagerClientImpl::::OnResult result=" << result;
    DCHECK(result);
  }

  // IViewManagerClient:
  virtual void OnViewManagerConnectionEstablished(
      uint16_t connection_id,
      uint32_t next_server_change_id,
      mojo::Array<view_manager::INodePtr> nodes) OVERRIDE;
  virtual void OnRootsAdded(Array<view_manager::INodePtr> nodes) OVERRIDE {
    NOTREACHED();
  }
  virtual void OnServerChangeIdAdvanced(
      uint32_t next_server_change_id) OVERRIDE {
  }
  virtual void OnNodeBoundsChanged(uint32_t node,
                                   mojo::RectPtr old_bounds,
                                   mojo::RectPtr new_bounds) OVERRIDE {
  }
  virtual void OnNodeHierarchyChanged(
      uint32_t node,
      uint32_t new_parent,
      uint32_t old_parent,
      uint32_t server_change_id,
      mojo::Array<view_manager::INodePtr> nodes) OVERRIDE {
  }
  virtual void OnNodeDeleted(uint32_t node, uint32_t server_change_id)
      OVERRIDE {
  }
  virtual void OnNodeViewReplaced(uint32_t node,
                                  uint32_t new_view_id,
                                  uint32_t old_view_id) OVERRIDE {
  }
  virtual void OnViewDeleted(uint32_t view) OVERRIDE {
  }
  virtual void OnViewInputEvent(uint32_t view_id,
                                EventPtr event,
                                const Callback<void()>& callback) OVERRIDE {
  }

  AuraDemo* aura_demo_;

  DISALLOW_COPY_AND_ASSIGN(IViewManagerClientImpl);
};

class AuraDemo : public Application {
 public:
  AuraDemo() {
    AddService<IViewManagerClientImpl>(this);
  }
  virtual ~AuraDemo() {}

  void SetRoot(view_manager::IViewManager* view_manager, uint32_t node_id) {
    context_factory_.reset(
        new ContextFactoryViewManager(view_manager, node_id));
    aura::Env::CreateInstance(true);
    aura::Env::GetInstance()->set_context_factory(context_factory_.get());
    screen_.reset(ScreenMojo::Create());
    gfx::Screen::SetScreenInstance(gfx::SCREEN_TYPE_NATIVE, screen_.get());

    window_tree_host_.reset(new WindowTreeHostViewManager(gfx::Rect(800, 600)));
    window_tree_host_->InitHost();

    window_tree_client_.reset(
        new DemoWindowTreeClient(window_tree_host_->window()));

    delegate1_.reset(new DemoWindowDelegate(SK_ColorBLUE));
    window1_ = new aura::Window(delegate1_.get());
    window1_->Init(aura::WINDOW_LAYER_TEXTURED);
    window1_->SetBounds(gfx::Rect(100, 100, 400, 400));
    window1_->Show();
    window_tree_host_->window()->AddChild(window1_);

    delegate2_.reset(new DemoWindowDelegate(SK_ColorRED));
    window2_ = new aura::Window(delegate2_.get());
    window2_->Init(aura::WINDOW_LAYER_TEXTURED);
    window2_->SetBounds(gfx::Rect(200, 200, 350, 350));
    window2_->Show();
    window_tree_host_->window()->AddChild(window2_);

    delegate21_.reset(new DemoWindowDelegate(SK_ColorGREEN));
    window21_ = new aura::Window(delegate21_.get());
    window21_->Init(aura::WINDOW_LAYER_TEXTURED);
    window21_->SetBounds(gfx::Rect(10, 10, 50, 50));
    window21_->Show();
    window2_->AddChild(window21_);

    window_tree_host_->Show();
  }

  virtual void Initialize() OVERRIDE {
  }

  scoped_ptr<ContextFactoryViewManager> context_factory_;

  scoped_ptr<ScreenMojo> screen_;

  scoped_ptr<DemoWindowTreeClient> window_tree_client_;

  scoped_ptr<DemoWindowDelegate> delegate1_;
  scoped_ptr<DemoWindowDelegate> delegate2_;
  scoped_ptr<DemoWindowDelegate> delegate21_;

  aura::Window* window1_;
  aura::Window* window2_;
  aura::Window* window21_;

  scoped_ptr<aura::WindowTreeHost> window_tree_host_;

  DISALLOW_COPY_AND_ASSIGN(AuraDemo);
};

void IViewManagerClientImpl::OnViewManagerConnectionEstablished(
      uint16_t connection_id,
      uint32_t next_server_change_id,
      mojo::Array<view_manager::INodePtr> nodes) {
  const uint32_t view_id = connection_id << 16 | 1;
  client()->CreateView(view_id, base::Bind(&IViewManagerClientImpl::OnResult,
                                           base::Unretained(this)));
  client()->SetView(nodes[0]->node_id, view_id,
                    base::Bind(&IViewManagerClientImpl::OnResult,
                               base::Unretained(this)));

  aura_demo_->SetRoot(client(), view_id);
}

}  // namespace examples

// static
Application* Application::Create() {
  return new examples::AuraDemo();
}

}  // namespace mojo

