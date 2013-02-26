// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/test_browser_window.h"

#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "ui/gfx/rect.h"

using content::NativeWebKeyboardEvent;

TestBrowserWindow::TestBrowserWindow() {}

TestBrowserWindow::~TestBrowserWindow() {}

bool TestBrowserWindow::IsActive() const {
  return false;
}

bool TestBrowserWindow::IsAlwaysOnTop() const {
  return false;
}

gfx::NativeWindow TestBrowserWindow::GetNativeWindow() {
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

#if defined(OS_WIN)
bool TestBrowserWindow::IsInMetroSnapMode() const {
  return false;
}
#endif

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

bool TestBrowserWindow::IsDownloadShelfVisible() const {
  return false;
}

DownloadShelf* TestBrowserWindow::GetDownloadShelf() {
  return &download_shelf_;
}

int TestBrowserWindow::GetExtraRenderViewHeight() const {
  return 0;
}

#if defined(OS_MACOSX)
bool TestBrowserWindow::IsFullscreenWithChrome() {
  return false;
}

bool TestBrowserWindow::IsFullscreenWithoutChrome() {
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

bool TestBrowserWindow::GetConstrainedWindowTopY(int* top_y) {
  return false;
}

namespace chrome {

namespace {

// Handles destroying a TestBrowserWindow when the Browser it is attached to is
// destroyed.
class TestBrowserWindowOwner : public chrome::BrowserListObserver {
 public:
  explicit TestBrowserWindowOwner(TestBrowserWindow* window) : window_(window) {
    BrowserList::AddObserver(this);
  }
  virtual ~TestBrowserWindowOwner() {
    BrowserList::RemoveObserver(this);
  }

 private:
  // Overridden from BrowserListObserver:
  virtual void OnBrowserRemoved(Browser* browser) OVERRIDE {
    if (browser->window() == window_.get())
      delete this;
  }

  scoped_ptr<TestBrowserWindow> window_;

  DISALLOW_COPY_AND_ASSIGN(TestBrowserWindowOwner);
};

}  // namespace

Browser* CreateBrowserWithTestWindowForParams(Browser::CreateParams* params) {
  TestBrowserWindow* window = new TestBrowserWindow;
  new TestBrowserWindowOwner(window);
  params->window = window;
  return new Browser(*params);
}

}  // namespace chrome
