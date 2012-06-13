// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_SCREEN_DIMMER_H_
#define ASH_WM_SCREEN_DIMMER_H_
#pragma once

#include "ash/ash_export.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "ui/aura/root_window_observer.h"

namespace aura {
class RootWindow;
}

namespace ui {
class Layer;
}

namespace ash {
namespace internal {

// ScreenDimmer displays a partially-opaque layer above everything
// else in the root window to darken the monitor.  It shouldn't be used
// for long-term brightness adjustments due to performance
// considerations -- it's only intended for cases where we want to
// briefly dim the screen (e.g. to indicate to the user that we're
// about to suspend a machine that lacks an internal backlight that
// can be adjusted).
class ASH_EXPORT ScreenDimmer : public aura::RootWindowObserver {
 public:
  class TestApi {
   public:
    explicit TestApi(ScreenDimmer* dimmer) : dimmer_(dimmer) {}

    ui::Layer* layer() { return dimmer_->dimming_layer_.get(); }

   private:
    ScreenDimmer* dimmer_;  // not owned

    DISALLOW_COPY_AND_ASSIGN(TestApi);
  };

  explicit ScreenDimmer(aura::RootWindow* root_window);
  virtual ~ScreenDimmer();

  // Dim or undim the root window.
  void SetDimming(bool should_dim);

  // aura::RootWindowObserver overrides:
  virtual void OnRootWindowResized(const aura::RootWindow* root,
                                   const gfx::Size& old_size) OVERRIDE;

 private:
  friend class TestApi;

  aura::RootWindow* root_window_;

  // Partially-opaque layer that's stacked above all of the root window's
  // children and used to dim the screen.  NULL until the first time we dim.
  scoped_ptr<ui::Layer> dimming_layer_;

  // Are we currently dimming the screen?
  bool currently_dimming_;

  DISALLOW_COPY_AND_ASSIGN(ScreenDimmer);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_WM_SCREEN_DIMMER_H_
