// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "app/gfx/canvas.h"
#include "app/menus/simple_menu_model.h"
#include "app/theme_provider.h"
#include "base/command_line.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/browser/chromeos/chromeos_browser_view.h"
#include "chrome/browser/chromeos/compact_location_bar_host.h"
#include "chrome/browser/chromeos/compact_navigation_bar.h"
#include "chrome/browser/chromeos/main_menu.h"
#include "chrome/browser/chromeos/status_area_view.h"
#include "chrome/browser/views/frame/browser_extender.h"
#include "chrome/browser/views/frame/browser_frame_gtk.h"
#include "chrome/browser/views/frame/browser_view.h"
#include "chrome/browser/views/tabs/tab_strip.h"
#include "chrome/browser/views/toolbar_view.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/x11_util.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "views/controls/button/button.h"
#include "views/controls/button/image_button.h"
#include "views/controls/menu/menu_2.h"

namespace {

// NormalExtender adds ChromeOS specific controls and menus to a BrowserView
// created with Browser::TYPE_NORMAL. This extender adds controls to
// the title bar as follows:
//                  ____  __ __
//      [MainMenu] /    \   \  \     [StatusArea]
//
// and adds the system context menu to the remaining arae of the titlebar.
//
// For Browser::TYPE_POPUP type of BrowserView, see PopupExtender class below.
class NormalExtender : public BrowserExtender {
 public:
  explicit NormalExtender(chromeos::ChromeosBrowserView* browser_view)
      : BrowserExtender(),
        browser_view_(browser_view) {
  }
  virtual ~NormalExtender() {}

 protected:

  virtual bool ShouldForceMaximizedWindow() {
    return browser_view_->ShouldForceMaximizedWindow();
  }

  virtual int GetMainMenuWidth() const {
    return browser_view_->GetMainMenuWidth();
  }

 private:
  chromeos::ChromeosBrowserView* browser_view_;

  DISALLOW_COPY_AND_ASSIGN(NormalExtender);
};

// PopupExtender class creates dedicated title window for popup window.
// The size and location of the created title window is controlled by
// by window manager.
class PopupExtender : public BrowserExtender {
 public:
  explicit PopupExtender()
      : BrowserExtender() {
  }
  virtual ~PopupExtender() {}

 private:

  virtual bool ShouldForceMaximizedWindow() {
    return false;
  }

  virtual int GetMainMenuWidth() const {
    return 0;
  }

  DISALLOW_COPY_AND_ASSIGN(PopupExtender);
};

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// BrowserExtender, public:

// static
BrowserExtender* BrowserExtender::Create(BrowserView* browser_view) {
  if (browser_view->browser()->type() & Browser::TYPE_POPUP)
    return new PopupExtender();
  else
    return new NormalExtender(
        static_cast<chromeos::ChromeosBrowserView*>(browser_view));
}
