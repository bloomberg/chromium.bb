// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/test_browser_window.h"

#include "ui/gfx/rect.h"

using content::NativeWebKeyboardEvent;

TestBrowserWindow::TestBrowserWindow(Browser* browser) {}

TestBrowserWindow::~TestBrowserWindow() {}

bool TestBrowserWindow::IsActive() const {
  return false;
}

bool TestBrowserWindow::IsAlwaysOnTop() const {
  return false;
}

gfx::NativeWindow TestBrowserWindow::GetNativeHandle() {
  return NULL;
}

BrowserWindowTesting* TestBrowserWindow::GetBrowserWindowTesting() {
  return NULL;
}

StatusBubble* TestBrowserWindow::GetStatusBubble() {
  return NULL;
}

gfx::Rect TestBrowserWindow::GetRestoredBounds() const {
  return gfx::Rect();
}

gfx::Rect TestBrowserWindow::GetBounds() const {
  return gfx::Rect();
}

bool TestBrowserWindow::IsMaximized() const {
  return false;
}

bool TestBrowserWindow::IsMinimized() const {
  return false;
}

bool TestBrowserWindow::IsFullscreen() const {
  return false;
}

bool TestBrowserWindow::IsFullscreenBubbleVisible() const {
  return false;
}

LocationBar* TestBrowserWindow::GetLocationBar() const {
  return const_cast<TestLocationBar*>(&location_bar_);
}

bool TestBrowserWindow::PreHandleKeyboardEvent(
    const NativeWebKeyboardEvent& event,
    bool* is_keyboard_shortcut) {
  return false;
}

bool TestBrowserWindow::IsBookmarkBarVisible() const {
  return false;
}

bool TestBrowserWindow::IsBookmarkBarAnimating() const {
  return false;
}

bool TestBrowserWindow::IsTabStripEditable() const {
  return false;
}

bool TestBrowserWindow::IsToolbarVisible() const {
  return false;
}

gfx::Rect TestBrowserWindow::GetRootWindowResizerRect() const {
  return gfx::Rect();
}

bool TestBrowserWindow::IsPanel() const {
  return false;
}

void TestBrowserWindow::ShowAboutChromeDialog() {
  return;
}

bool TestBrowserWindow::IsDownloadShelfVisible() const {
  return false;
}

DownloadShelf* TestBrowserWindow::GetDownloadShelf() {
  return NULL;
}

int TestBrowserWindow::GetExtraRenderViewHeight() const {
  return 0;
}

#if defined(OS_MACOSX)
bool TestBrowserWindow::InPresentationMode() {
  return false;
}
#endif

gfx::Rect TestBrowserWindow::GetInstantBounds() {
  return gfx::Rect();
}

WindowOpenDisposition TestBrowserWindow::GetDispositionForPopupBounds(
    const gfx::Rect& bounds) {
  return NEW_POPUP;
}

FindBar* TestBrowserWindow::CreateFindBar() {
  return NULL;
}
