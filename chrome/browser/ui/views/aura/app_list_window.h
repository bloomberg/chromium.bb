// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AURA_APP_LIST_WINDOW_H_
#define CHROME_BROWSER_UI_VIEWS_AURA_APP_LIST_WINDOW_H_
#pragma once

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/tab_first_render_watcher.h"
#include "ui/aura/desktop_observer.h"
#include "ui/gfx/compositor/layer_animation_observer.h"
#include "views/widget/widget_delegate.h"

class DOMView;

namespace views {
class Widget;
}

class AppListWindow : public views::WidgetDelegate,
                      public aura::DesktopObserver,
                      public ui::LayerAnimationObserver,
                      public TabFirstRenderWatcher::Delegate {
 public:
  // Show/hide app list window.
  static void SetVisible(bool visible);

  // Whether app list window is visible (shown or being shown).
  static bool IsVisible();

 private:
  AppListWindow();
  virtual ~AppListWindow();

  // views::WidgetDelegate overrides:
  virtual void DeleteDelegate() OVERRIDE;
  virtual views::View* GetContentsView() OVERRIDE;
  virtual void WindowClosing() OVERRIDE;
  virtual views::Widget* GetWidget() OVERRIDE;
  virtual const views::Widget* GetWidget() const OVERRIDE;

  // aura::DesktopObserver overrides:
  virtual void OnActiveWindowChanged(aura::Window* active) OVERRIDE;

  // ui::LayerAnimationObserver overrides:
  virtual void OnLayerAnimationEnded(
      const ui::LayerAnimationSequence* sequence) OVERRIDE;
  virtual void OnLayerAnimationAborted(
      const ui::LayerAnimationSequence* sequence) OVERRIDE;
  virtual void OnLayerAnimationScheduled(
      const ui::LayerAnimationSequence* sequence) OVERRIDE;

  // TabFirstRenderWatcher::Delegate implementation:
  virtual void OnRenderHostCreated(RenderViewHost* host) OVERRIDE;
  virtual void OnTabMainFrameLoaded() OVERRIDE;
  virtual void OnTabMainFrameFirstRender() OVERRIDE;

  // Initializes the window.
  void Init();

  // Shows/hides the window.
  void DoSetVisible(bool visible);

  // Current visible app list window.
  static AppListWindow* instance_;

  views::Widget* widget_;
  DOMView* contents_;
  bool is_visible_;

  // Monitors TabContents and set |content_rendered_| flag when it's rendered.
  scoped_ptr<TabFirstRenderWatcher> tab_watcher_;

  // Flag of whether the web contents is rendered. Showing animation is
  // deferred until this flag is set to true.
  bool content_rendered_;

  DISALLOW_COPY_AND_ASSIGN(AppListWindow);
};

#endif  // CHROME_BROWSER_UI_VIEWS_AURA_APP_LIST_WINDOW_H_
