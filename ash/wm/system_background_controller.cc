// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/system_background_controller.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/aura/root_window.h"
#include "ui/base/layout.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_type.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_rep.h"

namespace ash {
namespace internal {

namespace {

#if defined(OS_CHROMEOS)
// Background color used for the Chrome OS boot splash screen.
const SkColor kChromeOsBootColor = SkColorSetARGB(0xff, 0xfe, 0xfe, 0xfe);
#endif

}  // namespace

// ui::LayerDelegate that copies the aura host window's content to a ui::Layer.
class SystemBackgroundController::HostContentLayerDelegate
    : public ui::LayerDelegate {
 public:
  explicit HostContentLayerDelegate(aura::RootWindow* root_window)
      : root_window_(root_window) {
  }

  virtual ~HostContentLayerDelegate() {}

  // ui::LayerDelegate overrides:
  virtual void OnPaintLayer(gfx::Canvas* canvas) OVERRIDE {
    // It'd be safer to copy the area to a canvas in the constructor and then
    // copy from that canvas to this one here, but this appears to work (i.e. we
    // only call this before we draw our first frame) and it saves us an extra
    // copy.
    root_window_->CopyAreaToSkCanvas(
        gfx::Rect(root_window_->GetHostOrigin(), root_window_->GetHostSize()),
        gfx::Point(), canvas->sk_canvas());
  }

  virtual void OnDeviceScaleFactorChanged(float device_scale_factor) OVERRIDE {}

  virtual base::Closure PrepareForLayerBoundsChange() OVERRIDE {
    return base::Closure();
  }

 private:
  aura::RootWindow* root_window_;  // not owned

  DISALLOW_COPY_AND_ASSIGN(HostContentLayerDelegate);
};

SystemBackgroundController::SystemBackgroundController(
    aura::RootWindow* root_window,
    Content initial_content)
    : root_window_(root_window),
      content_(CONTENT_INVALID) {
  root_window_->AddRootWindowObserver(this);
  SetContent(initial_content);
}

SystemBackgroundController::~SystemBackgroundController() {
  root_window_->RemoveRootWindowObserver(this);
}

void SystemBackgroundController::SetContent(Content new_content) {
  if (new_content == content_)
    return;

  content_ = new_content;
  switch (new_content) {
    case CONTENT_COPY_FROM_HOST:
      CreateTexturedLayerWithHostContent();
      break;
    case CONTENT_BLACK:
      CreateColoredLayer(SK_ColorBLACK);
      break;
#if defined(OS_CHROMEOS)
    case CONTENT_CHROME_OS_BOOT_COLOR:
      CreateColoredLayer(kChromeOsBootColor);
      break;
#endif
    case CONTENT_INVALID:
      DCHECK(false) << "Invalid background layer content";
  }
}

void SystemBackgroundController::OnRootWindowResized(
    const aura::RootWindow* root,
    const gfx::Size& old_size) {
  DCHECK_EQ(root_window_, root);
  if (layer_.get())
    UpdateLayerBounds(layer_.get());
}

void SystemBackgroundController::CreateTexturedLayerWithHostContent() {
  layer_.reset();
  layer_delegate_.reset(new HostContentLayerDelegate(root_window_));
  layer_.reset(new ui::Layer(ui::LAYER_TEXTURED));
  layer_->set_delegate(layer_delegate_.get());
  UpdateLayerBounds(layer_.get());
  AddLayerToRootLayer(layer_.get());
}

void SystemBackgroundController::CreateColoredLayer(SkColor color) {
  layer_.reset();
  layer_delegate_.reset();
  layer_.reset(new ui::Layer(ui::LAYER_SOLID_COLOR));
  layer_->SetColor(color);
  UpdateLayerBounds(layer_.get());
  AddLayerToRootLayer(layer_.get());
}

void SystemBackgroundController::UpdateLayerBounds(ui::Layer* layer) {
  layer->SetBounds(gfx::Rect(root_window_->layer()->bounds().size()));
}

void SystemBackgroundController::AddLayerToRootLayer(ui::Layer* layer) {
  ui::Layer* root_layer = root_window_->layer();
  root_layer->Add(layer);
  root_layer->StackAtBottom(layer);
}

}  // namespace internal
}  // namespace ash
