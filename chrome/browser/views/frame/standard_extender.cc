// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/frame/browser_extender.h"

#include "chrome/browser/views/frame/browser_view.h"

namespace {

// StandardExtender for non ChromeOS build. This currently adds/does nothing.
// TODO(oshima): Add MainMenu support with a command line flag.
class StandardExtender : public BrowserExtender {
 public:
  StandardExtender() : BrowserExtender() {
  }
  virtual ~StandardExtender() {}

 private:
  // BrowserExtender overrides.
  virtual bool NonClientHitTest(const gfx::Point& point) { return false; }
  virtual bool ShouldForceMaximizedWindow() { return false; }
  virtual int GetMainMenuWidth() const { return 0; }

  DISALLOW_COPY_AND_ASSIGN(StandardExtender);
};

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// BrowserExtender, public:

// static
BrowserExtender* BrowserExtender::Create(BrowserView* browser_view) {
  return new StandardExtender();
}
