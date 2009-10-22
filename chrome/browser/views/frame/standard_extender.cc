// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/frame/browser_extender.h"

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
  virtual bool NonClientHitTest(const gfx::Point& point) { return false; }
  virtual void Show() {}
  virtual void Close() {}
  virtual void UpdateTitleBar() {}
  virtual void ActivationChanged() {}

  DISALLOW_COPY_AND_ASSIGN(StandardExtender);
};

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// BrowserExtender, public:

// static
BrowserExtender* BrowserExtender::Create(BrowserView* browser_view) {
  return new StandardExtender(browser_view);
}
