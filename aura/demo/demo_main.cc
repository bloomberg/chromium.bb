// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "aura/desktop.h"
#include "aura/event.h"
#include "aura/window.h"
#include "aura/window_delegate.h"
#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/i18n/icu_util.h"
#include "base/memory/scoped_ptr.h"
#include "third_party/skia/include/core/SkXfermode.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_paths.h"
#include "ui/gfx/canvas_skia.h"
#include "ui/gfx/rect.h"

#if defined(USE_X11)
#include "aura/hit_test.h"
#include "base/message_pump_x.h"
#endif

namespace {

// Trivial WindowDelegate implementation that draws a colored background.
class DemoWindowDelegate : public aura::WindowDelegate {
 public:
  explicit DemoWindowDelegate(SkColor color) : color_(color) {}

  virtual void OnFocus() OVERRIDE {}
  virtual void OnBlur() OVERRIDE {}
  virtual bool OnKeyEvent(aura::KeyEvent* event) OVERRIDE {
    return false;
  }

  virtual int GetNonClientComponent(const gfx::Point& point) const OVERRIDE {
    return HTCAPTION;
  }

  virtual bool OnMouseEvent(aura::MouseEvent* event) OVERRIDE {
    return true;
  }

  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE {
    canvas->AsCanvasSkia()->drawColor(color_, SkXfermode::kSrc_Mode);
  }

  virtual void OnWindowDestroyed() OVERRIDE {
  }

 private:
  SkColor color_;

  DISALLOW_COPY_AND_ASSIGN(DemoWindowDelegate);
};


}  // namespace

int main(int argc, char** argv) {
  CommandLine::Init(argc, argv);

  // The exit manager is in charge of calling the dtors of singleton objects.
  base::AtExitManager exit_manager;

  ui::RegisterPathProvider();
  icu_util::Initialize();
  ResourceBundle::InitSharedInstance("en-US");

#if defined(USE_X11)
  base::MessagePumpX::DisableGtkMessagePump();
#endif

  aura::Desktop::GetInstance();

  // Create a hierarchy of test windows.
  DemoWindowDelegate window_delegate1(SK_ColorBLUE);
  aura::Window window1(&window_delegate1);
  window1.set_id(1);
  window1.Init();
  window1.SetBounds(gfx::Rect(100, 100, 400, 400), 0);
  window1.SetVisibility(aura::Window::VISIBILITY_SHOWN);
  window1.SetParent(NULL);

  DemoWindowDelegate window_delegate2(SK_ColorRED);
  aura::Window window2(&window_delegate2);
  window2.set_id(2);
  window2.Init();
  window2.SetBounds(gfx::Rect(200, 200, 350, 350), 0);
  window2.SetVisibility(aura::Window::VISIBILITY_SHOWN);
  window2.SetParent(NULL);

  DemoWindowDelegate window_delegate3(SK_ColorGREEN);
  aura::Window window3(&window_delegate3);
  window3.set_id(3);
  window3.Init();
  window3.SetBounds(gfx::Rect(10, 10, 50, 50), 0);
  window3.SetVisibility(aura::Window::VISIBILITY_SHOWN);
  window3.SetParent(&window2);

  aura::Desktop::GetInstance()->Run();
  return 0;
}

