// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_FIND_BAR_FIND_BAR_BRIDGE_H_
#define CHROME_BROWSER_UI_COCOA_FIND_BAR_FIND_BAR_BRIDGE_H_

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "chrome/browser/ui/find_bar/find_bar.h"

class Browser;
class FindBarController;

// This class is included by find_bar_host_browsertest.cc, so it has to be
// objc-free.
#ifdef __OBJC__
@class FindBarCocoaController;
#else
class FindBarCocoaController;
#endif

// Implementation of FindBar for the Mac.  This class simply passes
// each message along to |cocoa_controller_|.
//
// The initialization here is a bit complicated.  FindBarBridge is
// created by a static method in BrowserWindow.  The FindBarBridge
// constructor creates a FindBarCocoaController, which in turn loads a
// FindBarView from a nib file.  All of this is happening outside of
// the main view hierarchy, so the static method also calls
// BrowserWindowCocoa::AddFindBar() in order to add its FindBarView to
// the cocoa views hierarchy.
//
// Memory ownership is relatively straightforward.  The FindBarBridge
// object is owned by the Browser.  FindBarCocoaController is retained
// by bother FindBarBridge and BrowserWindowController, since both use it.

class FindBarBridge : public FindBar,
                      public FindBarTesting {
 public:
  FindBarBridge(Browser* browser);
  virtual ~FindBarBridge();

  FindBarCocoaController* find_bar_cocoa_controller() {
    return cocoa_controller_;
  }

  virtual void SetFindBarController(
      FindBarController* find_bar_controller) override;

  virtual FindBarController* GetFindBarController() const override;

  virtual FindBarTesting* GetFindBarTesting() override;

  // Methods from FindBar.
  virtual void Show(bool animate) override;
  virtual void Hide(bool animate) override;
  virtual void SetFocusAndSelection() override;
  virtual void ClearResults(const FindNotificationDetails& results) override;
  virtual void StopAnimation() override;
  virtual void SetFindTextAndSelectedRange(
      const base::string16& find_text,
      const gfx::Range& selected_range) override;
  virtual base::string16 GetFindText() override;
  virtual gfx::Range GetSelectedRange() override;
  virtual void UpdateUIForFindResult(const FindNotificationDetails& result,
                                     const base::string16& find_text) override;
  virtual void AudibleAlert() override;
  virtual bool IsFindBarVisible() override;
  virtual void RestoreSavedFocus() override;
  virtual bool HasGlobalFindPasteboard() override;
  virtual void UpdateFindBarForChangedWebContents() override;
  virtual void MoveWindowIfNecessary(const gfx::Rect& selection_rect,
                                     bool no_redraw) override;

  // Methods from FindBarTesting.
  virtual bool GetFindBarWindowInfo(gfx::Point* position,
                                    bool* fully_visible) override;
  virtual base::string16 GetFindSelectedText() override;
  virtual base::string16 GetMatchCountText() override;
  virtual int GetWidth() override;

  // Used to disable find bar animations when testing.
  static bool disable_animations_during_testing_;

 private:
  // Pointer to the cocoa controller which manages the cocoa view.  Is
  // never nil.
  FindBarCocoaController* cocoa_controller_;

  // Pointer back to the owning controller.
  FindBarController* find_bar_controller_;  // weak, owns us

  DISALLOW_COPY_AND_ASSIGN(FindBarBridge);
};

#endif  // CHROME_BROWSER_UI_COCOA_FIND_BAR_FIND_BAR_BRIDGE_H_
