// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PANELS_NATIVE_PANEL_COCOA_H_
#define CHROME_BROWSER_UI_PANELS_NATIVE_PANEL_COCOA_H_
#pragma once

#include "chrome/browser/ui/panels/native_panel.h"

// Cocoa NativePanel interface used by the PanelWindowControllerCocoa class
// as its C++ window shim. I wish I had a "friend @class" construct to make
// this interface non-public.
class NativePanelCocoa : public NativePanel {
 public:
  virtual ~NativePanelCocoa() {}

  virtual Panel* panel() const = 0;

  // Callback from cocoa panel window controller that native window was actually
  // closed. The window may not close right away because of onbeforeunload
  // handlers.
  virtual void DidCloseNativeWindow() = 0;
};

#endif  // CHROME_BROWSER_UI_PANELS_NATIVE_PANEL_H_
