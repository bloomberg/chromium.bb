// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_FRAME_PANEL_BROWSER_VIEW_H_
#define CHROME_BROWSER_CHROMEOS_FRAME_PANEL_BROWSER_VIEW_H_
#pragma once

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/chromeos/frame/panel_controller.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "ui/base/x/x11_util.h"

class Browser;

namespace chromeos {

class PanelController;

// A browser view that implements Panel specific behavior.
// NOTE: This inherits from ::BrowserView in chrome/browser/ui/views/frame/,
// not chromeos::BrowserView in chrome/browser/chromeos/frame/.
class PanelBrowserView : public ::BrowserView,
                         public PanelController::Delegate {
 public:
  explicit PanelBrowserView(Browser* browser);
  virtual ~PanelBrowserView();

  // BrowserView overrides.
  virtual void Show() OVERRIDE;
  virtual void ShowInactive() OVERRIDE;
  virtual void SetBounds(const gfx::Rect& bounds) OVERRIDE;
  virtual void Close() OVERRIDE;
  virtual void FlashFrame() OVERRIDE;
  virtual void UpdateTitleBar() OVERRIDE;
  virtual WindowOpenDisposition GetDispositionForPopupBounds(
      const gfx::Rect& bounds) OVERRIDE;
  virtual bool GetSavedWindowBounds(gfx::Rect* bounds) const OVERRIDE;
  virtual bool GetSavedMaximizedState(bool* maximized) const OVERRIDE;

  // views::Widget::Observer overrides.
  virtual void OnWidgetActivationChanged(views::Widget* widget,
                                         bool active) OVERRIDE;

  // BrowserView : TabStripModelObserver overrides.
  virtual void TabChangedAt(TabContentsWrapper* contents,
                            int index,
                            TabChangeType change_type) OVERRIDE;

  // PanelController::Delegate overrides.
  virtual string16 GetPanelTitle() OVERRIDE;
  virtual SkBitmap GetPanelIcon() OVERRIDE;
  virtual bool CanClosePanel() OVERRIDE;
  virtual void ClosePanel() OVERRIDE;
  virtual void ActivatePanel() OVERRIDE;

  // Specific to PanelBrowserView.
  void SetCreatorView(PanelBrowserView* creator);

 private:
  // Enforces the min, max, and default bounds.
  void LimitBounds(gfx::Rect* bounds) const;

  void InitPanelController(bool is_active);

  // Controls interactions with the window manager for popup panels.
  scoped_ptr<chromeos::PanelController> panel_controller_;

  // X id for the content window of the panel that created this
  // panel.  This tells ChromeOS that it should be created next to the
  // content window of this panel.
  XID creator_xid_;
  DISALLOW_COPY_AND_ASSIGN(PanelBrowserView);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_FRAME_PANEL_BROWSER_VIEW_H_
