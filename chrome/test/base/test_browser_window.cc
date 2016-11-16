// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/test_browser_window.h"

#include "base/memory/ptr_util.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "ui/gfx/geometry/rect.h"


// Helpers --------------------------------------------------------------------

namespace chrome {

std::unique_ptr<Browser> CreateBrowserWithTestWindowForParams(
    Browser::CreateParams* params) {
  TestBrowserWindow* window = new TestBrowserWindow;
  new TestBrowserWindowOwner(window);
  params->window = window;
  return base::MakeUnique<Browser>(*params);
}

}  // namespace chrome


// TestBrowserWindow::TestLocationBar -----------------------------------------

GURL TestBrowserWindow::TestLocationBar::GetDestinationURL() const {
  return GURL();
}

WindowOpenDisposition
    TestBrowserWindow::TestLocationBar::GetWindowOpenDisposition() const {
  return WindowOpenDisposition::CURRENT_TAB;
}

ui::PageTransition
    TestBrowserWindow::TestLocationBar::GetPageTransition() const {
  return ui::PAGE_TRANSITION_LINK;
}

bool TestBrowserWindow::TestLocationBar::ShowPageActionPopup(
    const extensions::Extension* extension, bool grant_active_tab) {
  return false;
}

const OmniboxView* TestBrowserWindow::TestLocationBar::GetOmniboxView() const {
  return NULL;
}

OmniboxView* TestBrowserWindow::TestLocationBar::GetOmniboxView() {
  return NULL;
}

LocationBarTesting*
    TestBrowserWindow::TestLocationBar::GetLocationBarForTesting() {
  return NULL;
}


// TestBrowserWindow ----------------------------------------------------------

TestBrowserWindow::TestBrowserWindow() {}

TestBrowserWindow::~TestBrowserWindow() {}

bool TestBrowserWindow::IsActive() const {
  return false;
}

bool TestBrowserWindow::IsAlwaysOnTop() const {
  return false;
}

gfx::NativeWindow TestBrowserWindow::GetNativeWindow() const {
  return NULL;
}

StatusBubble* TestBrowserWindow::GetStatusBubble() {
  return NULL;
}

gfx::Rect TestBrowserWindow::GetRestoredBounds() const {
  return gfx::Rect();
}

ui::WindowShowState TestBrowserWindow::GetRestoredState() const {
  return ui::SHOW_STATE_DEFAULT;
}

gfx::Rect TestBrowserWindow::GetBounds() const {
  return gfx::Rect();
}

gfx::Size TestBrowserWindow::GetContentsSize() const {
  return gfx::Size();
}

bool TestBrowserWindow::IsMaximized() const {
  return false;
}

bool TestBrowserWindow::IsMinimized() const {
  return false;
}

bool TestBrowserWindow::ShouldHideUIForFullscreen() const {
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

ToolbarActionsBar* TestBrowserWindow::GetToolbarActionsBar() {
  return nullptr;
}

bool TestBrowserWindow::PreHandleKeyboardEvent(
    const content::NativeWebKeyboardEvent& event,
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

ShowTranslateBubbleResult TestBrowserWindow::ShowTranslateBubble(
    content::WebContents* contents,
    translate::TranslateStep step,
    translate::TranslateErrors::Type error_type,
    bool is_user_gesture) {
  return ShowTranslateBubbleResult::SUCCESS;
}

autofill::SaveCardBubbleView* TestBrowserWindow::ShowSaveCreditCardBubble(
    content::WebContents* contents,
    autofill::SaveCardBubbleController* controller,
    bool user_gesture) {
  return nullptr;
}

bool TestBrowserWindow::IsDownloadShelfVisible() const {
  return false;
}

DownloadShelf* TestBrowserWindow::GetDownloadShelf() {
  return &download_shelf_;
}

WindowOpenDisposition TestBrowserWindow::GetDispositionForPopupBounds(
    const gfx::Rect& bounds) {
  return WindowOpenDisposition::NEW_POPUP;
}

FindBar* TestBrowserWindow::CreateFindBar() {
  return NULL;
}

web_modal::WebContentsModalDialogHost*
    TestBrowserWindow::GetWebContentsModalDialogHost() {
  return NULL;
}

int TestBrowserWindow::GetRenderViewHeightInsetWithDetachedBookmarkBar() {
  return 0;
}

void TestBrowserWindow::ExecuteExtensionCommand(
    const extensions::Extension* extension,
    const extensions::Command& command) {}

ExclusiveAccessContext* TestBrowserWindow::GetExclusiveAccessContext() {
  return nullptr;
}

std::string TestBrowserWindow::GetWorkspace() const {
  return std::string();
}

bool TestBrowserWindow::IsVisibleOnAllWorkspaces() const {
  return false;
}

// TestBrowserWindowOwner -----------------------------------------------------

TestBrowserWindowOwner::TestBrowserWindowOwner(TestBrowserWindow* window)
    : window_(window) {
  BrowserList::AddObserver(this);
}

TestBrowserWindowOwner::~TestBrowserWindowOwner() {
  BrowserList::RemoveObserver(this);
}

void TestBrowserWindowOwner::OnBrowserRemoved(Browser* browser) {
  if (browser->window() == window_.get())
    delete this;
}
