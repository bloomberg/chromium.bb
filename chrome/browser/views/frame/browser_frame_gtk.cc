// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/frame/browser_frame_gtk.h"

#include "base/logging.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/status_bubble.h"
#include "chrome/browser/views/frame/browser_root_view.h"
#include "chrome/browser/views/frame/browser_view.h"
#include "chrome/browser/views/frame/opaque_browser_frame_view.h"
#include "views/widget/root_view.h"
#include "views/window/hit_test.h"

namespace {

// BrowserNonClientFrameView implementation for popups. We let the window
// manager implementation render the decorations for popups, so this draws
// nothing.
class PopupNonClientFrameView : public BrowserNonClientFrameView {
 public:
  PopupNonClientFrameView() {
  }

  // NonClientFrameView:
  virtual gfx::Rect GetBoundsForClientView() const {
    return gfx::Rect(0, 0, width(), height());
  }
  virtual bool AlwaysUseCustomFrame() const { return false; }
  virtual bool AlwaysUseNativeFrame() const { return true; }
  virtual gfx::Rect GetWindowBoundsForClientBounds(
      const gfx::Rect& client_bounds) const {
    return client_bounds;
  }
  virtual gfx::Point GetSystemMenuPoint() const {
    // Never used on GTK.
    // TODO: make this method windows specific.
    return gfx::Point(0, 0);
  }
  virtual int NonClientHitTest(const gfx::Point& point) {
    return HTNOWHERE;
  }
  virtual void GetWindowMask(const gfx::Size& size,
                             gfx::Path* window_mask) {}
  virtual void EnableClose(bool enable) {}
  virtual void ResetWindowControls() {}

  // BrowserNonClientFrameView:
  virtual gfx::Rect GetBoundsForTabStrip(TabStrip* tabstrip) const {
    return gfx::Rect(0, 0, width(), tabstrip->GetPreferredHeight());
  }
  virtual void UpdateThrobber(bool running) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(PopupNonClientFrameView);
};

}

// static (Factory method.)
BrowserFrame* BrowserFrame::Create(BrowserView* browser_view,
                                   Profile* profile) {
  BrowserFrameGtk* frame = new BrowserFrameGtk(browser_view, profile);
  frame->Init();
  return frame;
}

BrowserFrameGtk::BrowserFrameGtk(BrowserView* browser_view, Profile* profile)
    : WindowGtk(browser_view),
      browser_view_(browser_view),
      browser_frame_view_(NULL),
      root_view_(NULL),
      profile_(profile) {
  browser_view_->set_frame(this);
  if (browser_view->browser()->type() == Browser::TYPE_POPUP)
    browser_frame_view_ = new PopupNonClientFrameView();
  else
    browser_frame_view_ = new OpaqueBrowserFrameView(this, browser_view_);
  GetNonClientView()->SetFrameView(browser_frame_view_);
  // Don't focus anything on creation, selecting a tab will set the focus.
}

BrowserFrameGtk::~BrowserFrameGtk() {
}

void BrowserFrameGtk::Init() {
  WindowGtk::Init(NULL, gfx::Rect());
}

views::Window* BrowserFrameGtk::GetWindow() {
  return this;
}

void BrowserFrameGtk::TabStripCreated(TabStrip* tabstrip) {
}

int BrowserFrameGtk::GetMinimizeButtonOffset() const {
  NOTIMPLEMENTED();
  return 0;
}

gfx::Rect BrowserFrameGtk::GetBoundsForTabStrip(TabStrip* tabstrip) const {
  return browser_frame_view_->GetBoundsForTabStrip(tabstrip);
}

void BrowserFrameGtk::UpdateThrobber(bool running) {
  browser_frame_view_->UpdateThrobber(running);
}

void BrowserFrameGtk::ContinueDraggingDetachedTab() {
  NOTIMPLEMENTED();
}

ThemeProvider* BrowserFrameGtk::GetThemeProviderForFrame() const {
  // This is implemented for a different interface than GetThemeProvider is,
  // but they mean the same things.
  return GetThemeProvider();
}

bool BrowserFrameGtk::AlwaysUseNativeFrame() const {
  return false;
}

ThemeProvider* BrowserFrameGtk::GetThemeProvider() const {
  return profile_->GetThemeProvider();
}

ThemeProvider* BrowserFrameGtk::GetDefaultThemeProvider() const {
  return profile_->GetThemeProvider();
}

views::RootView* BrowserFrameGtk::CreateRootView() {
  root_view_ = new BrowserRootView(browser_view_, this);
  return root_view_;
}

void BrowserFrameGtk::IsActiveChanged() {
  GetRootView()->SchedulePaint();
  browser_view_->ActivationChanged(IsActive());
  views::WidgetGtk::IsActiveChanged();
}

bool BrowserFrameGtk::GetAccelerator(int cmd_id,
                                     views::Accelerator* accelerator) {
  return browser_view_->GetAccelerator(cmd_id, accelerator);
}

gboolean BrowserFrameGtk::OnWindowStateEvent(GtkWidget* widget,
                                             GdkEventWindowState* event) {
  gboolean result = views::WindowGtk::OnWindowStateEvent(widget, event);
  if ((!IsVisible() || IsMinimized()) && browser_view_->GetStatusBubble()) {
    // The window is effectively hidden. We have to hide the status bubble as
    // unlike windows gtk has no notion of child windows that are hidden along
    // with the parent.
    browser_view_->GetStatusBubble()->Hide();
  }
  return result;
}

gboolean BrowserFrameGtk::OnConfigureEvent(GtkWidget* widget,
                                           GdkEventConfigure* event) {
  browser_view_->WindowMoved();
  return views::WindowGtk::OnConfigureEvent(widget, event);
}
