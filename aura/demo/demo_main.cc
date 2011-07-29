// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "aura/desktop.h"
#include "aura/desktop_host.h"
#include "aura/window.h"
#include "aura/window_delegate.h"
#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/i18n/icu_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "third_party/skia/include/core/SkXfermode.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/canvas_skia.h"
#include "ui/gfx/rect.h"
#include "ui/base/ui_base_paths.h"

namespace {

// Trivial WindowDelegate implementation that draws a blue background.
class DemoWindowDelegate : public aura::WindowDelegate {
 public:
  explicit DemoWindowDelegate(aura::Window* window);

  virtual void OnPaint(const gfx::Rect& bounds) OVERRIDE;

 private:
  aura::Window* window_;

  DISALLOW_COPY_AND_ASSIGN(DemoWindowDelegate);
};

DemoWindowDelegate::DemoWindowDelegate(aura::Window* window)
    : window_(window) {
}

void DemoWindowDelegate::OnPaint(const gfx::Rect& bounds) {
  scoped_ptr<gfx::Canvas> canvas(
      gfx::Canvas::CreateCanvas(bounds.width(), bounds.height(), false));
  canvas->AsCanvasSkia()->drawColor(
      SK_ColorBLUE, SkXfermode::kSrc_Mode);
  window_->SetCanvas(*canvas->AsCanvasSkia(), bounds.origin());
}

}  // namespace

int main(int argc, char** argv) {
  CommandLine::Init(argc, argv);

  // The exit manager is in charge of calling the dtors of singleton objects.
  base::AtExitManager exit_manager;

  ui::RegisterPathProvider();
  icu_util::Initialize();
  ResourceBundle::InitSharedInstance("en-US");

  // Create the DesktopHost and Desktop.
  scoped_ptr<aura::DesktopHost> host(
      aura::DesktopHost::Create(gfx::Rect(200, 200, 1024, 768)));
  aura::Desktop desktop(host->GetAcceleratedWidget(), host->GetSize());
  host->SetDesktop(&desktop);

  // Create a test window and give it a size.
  aura::Window window(&desktop);
  window.SetBounds(gfx::Rect(100, 100, 400, 400), 0);
  window.SetVisibility(aura::Window::VISIBILITY_SHOWN);
  DemoWindowDelegate window_delegate(&window);
  window.set_delegate(&window_delegate);

  host->Show();

  MessageLoop main_message_loop(MessageLoop::TYPE_UI);
  MessageLoopForUI::current()->Run(NULL);
  return 0;
}
