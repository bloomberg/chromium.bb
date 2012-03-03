// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_frame_aura.h"

#include "ash/ash_switches.h"
#include "ash/shell.h"
#include "base/command_line.h"
#include "chrome/browser/chromeos/status/status_area_view.h"
#include "chrome/browser/ui/views/ash/chrome_shell_delegate.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/gfx/font.h"
#include "ui/views/background.h"

namespace {

////////////////////////////////////////////////////////////////////////////////
// StatusAreaBoundsWatcher

class StatusAreaBoundsWatcher : public aura::WindowObserver {
 public:
  explicit StatusAreaBoundsWatcher(BrowserFrame* frame)
      : frame_(frame),
        status_area_window_(NULL) {
    StartWatch();
  }

  virtual ~StatusAreaBoundsWatcher() {
    StopWatch();
  }

 private:
  void StartWatch() {
    DCHECK(ChromeShellDelegate::instance());

    StatusAreaView* status_area =
        ChromeShellDelegate::instance()->GetStatusArea();
    if (!status_area)
      return;

    StopWatch();
    status_area_window_ = status_area->GetWidget()->GetNativeWindow();
    status_area_window_->AddObserver(this);
  }

  void StopWatch() {
    if (status_area_window_) {
      status_area_window_->RemoveObserver(this);
      status_area_window_ = NULL;
    }
  }

  // Overridden from aura::WindowObserver:
  virtual void OnWindowBoundsChanged(aura::Window* window,
                                     const gfx::Rect& bounds) OVERRIDE {
    DCHECK(window == status_area_window_);

    // Triggers frame layout when the bounds of status area changed.
    frame_->TabStripDisplayModeChanged();
  }

  virtual void OnWindowDestroyed(aura::Window* window) OVERRIDE {
    DCHECK(window == status_area_window_);
    status_area_window_ = NULL;
  }

  BrowserFrame* frame_;
  aura::Window* status_area_window_;

  DISALLOW_COPY_AND_ASSIGN(StatusAreaBoundsWatcher);
};

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// BrowserFrameAura::WindowPropertyWatcher

class BrowserFrameAura::WindowPropertyWatcher : public aura::WindowObserver {
 public:
  explicit WindowPropertyWatcher(BrowserFrameAura* browser_frame_aura,
                                 BrowserFrame* browser_frame)
      : browser_frame_aura_(browser_frame_aura),
        browser_frame_(browser_frame) {}

  virtual void OnWindowPropertyChanged(aura::Window* window,
                                       const void* key,
                                       intptr_t old) {
    if (key != aura::client::kShowStateKey)
      return;

    // Allow the frame to be replaced when maximizing an app.
    if (browser_frame_->non_client_view() &&
        browser_frame_aura_->browser_view()->browser()->is_app())
      browser_frame_->non_client_view()->UpdateFrame();

    // When migrating from regular ChromeOS to Aura, windows can have saved
    // restore bounds that are exactly equal to the maximized bounds.  Thus when
    // you hit maximize, there is no resize and the layout doesn't get
    // refreshed. This can also theoretically happen if a user drags a window to
    // 0,0 then resizes it to fill the workspace, then hits maximize.  We need
    // to force a layout on show state changes.  crbug.com/108073
    if (browser_frame_->non_client_view())
      browser_frame_->non_client_view()->Layout();

    // Watch for status area bounds change for maximized browser window in Aura
    // compact mode.
    if (ash::Shell::GetInstance()->IsWindowModeCompact() &&
        browser_frame_aura_->IsMaximized())
      status_area_watcher_.reset(new StatusAreaBoundsWatcher(browser_frame_));
    else
      status_area_watcher_.reset();
  }

 private:
  BrowserFrameAura* browser_frame_aura_;
  BrowserFrame* browser_frame_;
  scoped_ptr<StatusAreaBoundsWatcher> status_area_watcher_;

  DISALLOW_COPY_AND_ASSIGN(WindowPropertyWatcher);
};

///////////////////////////////////////////////////////////////////////////////
// BrowserFrameAura, public:

BrowserFrameAura::BrowserFrameAura(BrowserFrame* browser_frame,
                                   BrowserView* browser_view)
    : views::NativeWidgetAura(browser_frame),
      browser_view_(browser_view),
      window_property_watcher_(new WindowPropertyWatcher(this, browser_frame)) {
  GetNativeWindow()->SetName("BrowserFrameAura");
  GetNativeWindow()->AddObserver(window_property_watcher_.get());
}

BrowserFrameAura::~BrowserFrameAura() {
}

///////////////////////////////////////////////////////////////////////////////
// BrowserFrameAura, views::NativeWidgetAura overrides:

void BrowserFrameAura::OnWindowDestroying() {
  // Window is destroyed before our destructor is called, so clean up our
  // observer here.
  GetNativeWindow()->RemoveObserver(window_property_watcher_.get());
  views::NativeWidgetAura::OnWindowDestroying();
}

////////////////////////////////////////////////////////////////////////////////
// BrowserFrameAura, NativeBrowserFrame implementation:

views::NativeWidget* BrowserFrameAura::AsNativeWidget() {
  return this;
}

const views::NativeWidget* BrowserFrameAura::AsNativeWidget() const {
  return this;
}

int BrowserFrameAura::GetMinimizeButtonOffset() const {
  return 0;
}

void BrowserFrameAura::TabStripDisplayModeChanged() {
}

////////////////////////////////////////////////////////////////////////////////
// BrowserFrame, public:

// static
const gfx::Font& BrowserFrame::GetTitleFont() {
  static gfx::Font* title_font = new gfx::Font;
  return *title_font;
}

////////////////////////////////////////////////////////////////////////////////
// NativeBrowserFrame, public:

// static
NativeBrowserFrame* NativeBrowserFrame::CreateNativeBrowserFrame(
    BrowserFrame* browser_frame,
    BrowserView* browser_view) {
  return new BrowserFrameAura(browser_frame, browser_view);
}
