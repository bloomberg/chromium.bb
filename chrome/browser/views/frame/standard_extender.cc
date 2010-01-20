// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/frame/browser_extender.h"

#include "base/gfx/rect.h"
#include "chrome/browser/views/frame/browser_view.h"

class Tab;

namespace {

// StandardExtender for non ChromeOS build. This currently adds/does nothing.
// TODO(oshima): Add MainMenu support with a command line flag.
class StandardExtender : public BrowserExtender {
 public:
  explicit StandardExtender(BrowserView* browser_view)
      : BrowserExtender(browser_view) {
  }
  virtual ~StandardExtender() {}

 private:
  // BrowserExtender overrides.
  virtual void Init() {}
  virtual void Layout(const gfx::Rect& bounds,
                      gfx::Rect* tabstrip_bounds,
                      int* bottom) {
    NOTREACHED();
  }
  virtual bool NonClientHitTest(const gfx::Point& point) { return false; }
  virtual bool ShouldForceHideToolbar() { return false; }
  virtual bool ShouldForceMaximizedWindow() { return false; }
  virtual bool SetFocusToCompactNavigationBar() { return false; }
  virtual void ToggleCompactNavigationBar() {}
  virtual void OnMouseEnteredToTab(Tab* tab) {}
  virtual void OnMouseMovedOnTab(Tab* tab) {}
  virtual void OnMouseExitedFromTab(Tab* tab) {}
  virtual int GetMainMenuWidth() const { return 0; }

  DISALLOW_COPY_AND_ASSIGN(StandardExtender);
};

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// BrowserExtender, public:

// static
BrowserExtender* BrowserExtender::Create(BrowserView* browser_view) {
  return new StandardExtender(browser_view);
}
