// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/test_browser_window_aura.h"

namespace chrome {

scoped_ptr<Browser> CreateBrowserWithAuraTestWindowForParams(
    scoped_ptr<aura::Window> window,
    Browser::CreateParams* params) {
  if (window.get() == nullptr) {
    window.reset(new aura::Window(nullptr));
    window->set_id(0);
    window->SetType(ui::wm::WINDOW_TYPE_NORMAL);
    window->Init(ui::LAYER_TEXTURED);
    window->Show();
  }

  TestBrowserWindowAura* browser_window =
      new TestBrowserWindowAura(window.Pass());
  new TestBrowserWindowOwner(browser_window);
  return browser_window->CreateBrowser(params);
}

}  // namespace chrome

TestBrowserWindowAura::TestBrowserWindowAura(
    scoped_ptr<aura::Window> native_window)
    : native_window_(native_window.Pass()) {}

TestBrowserWindowAura::~TestBrowserWindowAura() {}

gfx::NativeWindow TestBrowserWindowAura::GetNativeWindow() const {
  return native_window_.get();
}

void TestBrowserWindowAura::Show() {
  native_window_->Show();
}

void TestBrowserWindowAura::Hide() {
  native_window_->Hide();
}

gfx::Rect TestBrowserWindowAura::GetBounds() const {
  return native_window_->bounds();
}

scoped_ptr<Browser> TestBrowserWindowAura::CreateBrowser(
    Browser::CreateParams* params) {
  params->window = this;
  browser_ = new Browser(*params);
  return make_scoped_ptr(browser_);
}
