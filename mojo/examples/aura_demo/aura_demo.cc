// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>
#include <string>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/message_loop/message_loop.h"
#include "mojo/examples/aura_demo/demo_screen.h"
#include "mojo/examples/aura_demo/root_window_host_mojo.h"
#include "mojo/public/bindings/allocation_scope.h"
#include "mojo/public/gles2/gles2_cpp.h"
#include "mojo/public/shell/application.h"
#include "mojo/public/system/core.h"
#include "mojo/public/system/macros.h"
#include "mojom/native_viewport.h"
#include "mojom/shell.h"
#include "ui/aura/client/default_capture_client.h"
#include "ui/aura/client/window_tree_client.h"
#include "ui/aura/env.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/base/hit_test.h"
#include "ui/gfx/canvas.h"

#if defined(WIN32)
#if !defined(CDECL)
#define CDECL __cdecl
#endif
#define AURA_DEMO_EXPORT __declspec(dllexport)
#else
#define CDECL
#define AURA_DEMO_EXPORT __attribute__((visibility("default")))
#endif

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
  virtual void OnWindowDestroying() OVERRIDE {}
  virtual void OnWindowDestroyed() OVERRIDE {}
  virtual void OnWindowTargetVisibilityChanged(bool visible) OVERRIDE {}
  virtual bool HasHitTestMask() const OVERRIDE { return false; }
  virtual void GetHitTestMask(gfx::Path* mask) const OVERRIDE {}
  virtual void DidRecreateLayer(ui::Layer* old_layer,
                                ui::Layer* new_layer) OVERRIDE {}

 private:
  SkColor color_;

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

class AuraDemo : public Application {
 public:
  explicit AuraDemo(MojoHandle shell_handle) : Application(shell_handle) {
    screen_.reset(DemoScreen::Create());
    gfx::Screen::SetScreenInstance(gfx::SCREEN_TYPE_NATIVE, screen_.get());

    InterfacePipe<NativeViewport, AnyInterface> pipe;

    mojo::AllocationScope scope;
    shell()->Connect("mojo:mojo_native_viewport_service",
                     pipe.handle_to_peer.Pass());
    root_window_host_.reset(new WindowTreeHostMojo(
        pipe.handle_to_self.Pass(),
        gfx::Rect(800, 600),
        base::Bind(&AuraDemo::HostContextCreated, base::Unretained(this))));
  }

 private:
  void HostContextCreated() {
    aura::RootWindow::CreateParams params(
        gfx::Rect(root_window_host_->bounds().size()));
    params.host = root_window_host_.get();
    root_window_.reset(new aura::RootWindow(params));
    root_window_host_->set_delegate(root_window_.get());
    root_window_->Init();

    window_tree_client_.reset(new DemoWindowTreeClient(root_window_->window()));

    delegate1_.reset(new DemoWindowDelegate(SK_ColorBLUE));
    window1_ = new aura::Window(delegate1_.get());
    window1_->Init(aura::WINDOW_LAYER_TEXTURED);
    window1_->SetBounds(gfx::Rect(100, 100, 400, 400));
    window1_->Show();
    root_window_->window()->AddChild(window1_);

    delegate2_.reset(new DemoWindowDelegate(SK_ColorRED));
    window2_ = new aura::Window(delegate2_.get());
    window2_->Init(aura::WINDOW_LAYER_TEXTURED);
    window2_->SetBounds(gfx::Rect(200, 200, 350, 350));
    window2_->Show();
    root_window_->window()->AddChild(window2_);

    delegate21_.reset(new DemoWindowDelegate(SK_ColorGREEN));
    window21_ = new aura::Window(delegate21_.get());
    window21_->Init(aura::WINDOW_LAYER_TEXTURED);
    window21_->SetBounds(gfx::Rect(10, 10, 50, 50));
    window21_->Show();
    window2_->AddChild(window21_);

    root_window_->host()->Show();
  }

  scoped_ptr<DemoScreen> screen_;

  scoped_ptr<DemoWindowTreeClient> window_tree_client_;

  scoped_ptr<DemoWindowDelegate> delegate1_;
  scoped_ptr<DemoWindowDelegate> delegate2_;
  scoped_ptr<DemoWindowDelegate> delegate21_;

  aura::Window* window1_;
  aura::Window* window2_;
  aura::Window* window21_;

  scoped_ptr<WindowTreeHostMojo> root_window_host_;
  scoped_ptr<aura::RootWindow> root_window_;
};

}  // namespace examples
}  // namespace mojo

extern "C" AURA_DEMO_EXPORT MojoResult CDECL MojoMain(
    MojoHandle shell_handle) {
  CommandLine::Init(0, NULL);
  base::AtExitManager at_exit;
  base::MessageLoop loop;
  mojo::GLES2Initializer gles2;

  // TODO(beng): This crashes in a DCHECK on X11 because this thread's
  //             MessageLoop is not of TYPE_UI. I think we need a way to build
  //             Aura that doesn't define platform-specific stuff.
  aura::Env::CreateInstance();
  mojo::examples::AuraDemo app(shell_handle);
  loop.Run();

  return MOJO_RESULT_OK;
}
