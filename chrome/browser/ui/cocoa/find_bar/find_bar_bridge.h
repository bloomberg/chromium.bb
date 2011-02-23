// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_FIND_BAR_FIND_BAR_BRIDGE_H_
#define CHROME_BROWSER_UI_COCOA_FIND_BAR_FIND_BAR_BRIDGE_H_
#pragma once

#include "base/logging.h"
#include "chrome/browser/ui/find_bar/find_bar.h"

class BrowserWindowCocoa;
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
  FindBarBridge();
  virtual ~FindBarBridge();

  FindBarCocoaController* find_bar_cocoa_controller() {
    return cocoa_controller_;
  }

  virtual void SetFindBarController(FindBarController* find_bar_controller);

  virtual FindBarController* GetFindBarController() const;

  virtual FindBarTesting* GetFindBarTesting();

  // Methods from FindBar.
  virtual void Show(bool animate);
  virtual void Hide(bool animate);
  virtual void SetFocusAndSelection();
  virtual void ClearResults(const FindNotificationDetails& results);
  virtual void StopAnimation();
  virtual void SetFindText(const string16& find_text);
  virtual void UpdateUIForFindResult(const FindNotificationDetails& result,
                                     const string16& find_text);
  virtual void AudibleAlert();
  virtual bool IsFindBarVisible();
  virtual void RestoreSavedFocus();
  virtual void MoveWindowIfNecessary(const gfx::Rect& selection_rect,
                                     bool no_redraw);

  // Methods from FindBarTesting.
  virtual bool GetFindBarWindowInfo(gfx::Point* position,
                                    bool* fully_visible);
  virtual string16 GetFindText();
  virtual string16 GetFindSelectedText();
  virtual string16 GetMatchCountText();

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
