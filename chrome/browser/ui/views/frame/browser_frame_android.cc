// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_frame_android.h"

#include "chrome/browser/ui/views/frame/browser_shutdown.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host_platform.h"

///////////////////////////////////////////////////////////////////////////////
// BrowserFrameAndroid, public:

// static
const char BrowserFrameAndroid::kWindowName[] = "BrowserFrameAndroid";

namespace {
// |g_host| should be set before browser frame instantiation. It is used to get
// the window which is needed for creating the widget for the browser frame.
// TODO(moshayedi): crbug.com/551055. This is temporary until we have
// multi-window support.
aura::WindowTreeHostPlatform* g_host = nullptr;
}

BrowserFrameAndroid::BrowserFrameAndroid(BrowserFrame* browser_frame,
                                         BrowserView* browser_view)
    : views::NativeWidgetAura(browser_frame), browser_view_(browser_view) {
  GetNativeWindow()->SetName(kWindowName);
}

// static
void BrowserFrameAndroid::SetHost(aura::WindowTreeHostPlatform* host) {
  g_host = host;
}

///////////////////////////////////////////////////////////////////////////////
// BrowserFrameAndroid, views::NativeWidgetAura overrides:

void BrowserFrameAndroid::OnWindowDestroying(aura::Window* window) {
  // Destroy any remaining WebContents early on. Doing so may result in
  // calling back to one of the Views/LayoutManagers or supporting classes of
  // BrowserView. By destroying here we ensure all said classes are valid.
  DestroyBrowserWebContents(browser_view_->browser());
  NativeWidgetAura::OnWindowDestroying(window);
}

////////////////////////////////////////////////////////////////////////////////
// BrowserFrameAndroid, NativeBrowserFrame implementation:

views::Widget::InitParams BrowserFrameAndroid::GetWidgetParams() {
  DCHECK(g_host);

  views::Widget::InitParams params;
  params.native_widget = this;
  params.context = g_host->window();
  return params;
}

bool BrowserFrameAndroid::UseCustomFrame() const {
  return true;
}

bool BrowserFrameAndroid::UsesNativeSystemMenu() const {
  return false;
}

int BrowserFrameAndroid::GetMinimizeButtonOffset() const {
  return 0;
}

bool BrowserFrameAndroid::ShouldSaveWindowPlacement() const {
  return false;
}

void BrowserFrameAndroid::GetWindowPlacement(
    gfx::Rect* bounds,
    ui::WindowShowState* show_state) const {}

BrowserFrameAndroid::~BrowserFrameAndroid() {}
