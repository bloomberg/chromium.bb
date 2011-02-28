// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The LocationBar class is a virtual interface, defining access to the
// window's location bar component.  This class exists so that cross-platform
// components like the browser command system can talk to the platform
// specific implementations of the location bar control.  It also allows the
// location bar to be mocked for testing.

#ifndef CHROME_BROWSER_UI_OMNIBOX_LOCATION_BAR_H_
#define CHROME_BROWSER_UI_OMNIBOX_LOCATION_BAR_H_
#pragma once

#include <string>

#include "base/string16.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/common/page_transition_types.h"
#include "webkit/glue/window_open_disposition.h"

class AutocompleteEditView;
class ExtensionAction;
class InstantController;
class LocationBarTesting;
class TabContents;
class TabContentsWrapper;

class LocationBar {
 public:
  // Shows the first run information bubble anchored to the location bar.
  virtual void ShowFirstRunBubble(FirstRun::BubbleType bubble_type) = 0;

  // Sets the suggested text to show in the omnibox. This is shown in addition
  // to the current text of the omnibox.
  virtual void SetSuggestedText(const string16& text) = 0;

  // Returns the string of text entered in the location bar.
  virtual std::wstring GetInputString() const = 0;

  // Returns the WindowOpenDisposition that should be used to determine where
  // to open a URL entered in the location bar.
  virtual WindowOpenDisposition GetWindowOpenDisposition() const = 0;

  // Returns the PageTransition that should be recorded in history when the URL
  // entered in the location bar is loaded.
  virtual PageTransition::Type GetPageTransition() const = 0;

  // Accepts the current string of text entered in the location bar.
  virtual void AcceptInput() = 0;

  // Focuses the location bar.  Optionally also selects its contents.
  virtual void FocusLocation(bool select_all) = 0;

  // Clears the location bar, inserts an annoying little "?" turd and sets
  // focus to it.
  virtual void FocusSearch() = 0;

  // Updates the state of the images showing the content settings status.
  virtual void UpdateContentSettingsIcons() = 0;

  // Updates the state of the page actions.
  virtual void UpdatePageActions() = 0;

  // Called when the page-action data needs to be refreshed, e.g. when an
  // extension is unloaded or crashes.
  virtual void InvalidatePageActions() = 0;

  // Saves the state of the location bar to the specified TabContents, so that
  // it can be restored later. (Done when switching tabs).
  virtual void SaveStateToContents(TabContents* contents) = 0;

  // Reverts the location bar.  The bar's permanent text will be shown.
  virtual void Revert() = 0;

  // Returns a pointer to the text entry view.
  virtual const AutocompleteEditView* location_entry() const = 0;
  virtual AutocompleteEditView* location_entry() = 0;

  // Returns a pointer to the testing interface.
  virtual LocationBarTesting* GetLocationBarForTesting() = 0;

 protected:
  virtual ~LocationBar() {}

  // Updates instant in response to the text possibly changing in the edit.
  static void UpdateInstant(InstantController* instant,
                            TabContentsWrapper* tab,
                            AutocompleteEditView* edit,
                            string16* suggested_text);
};

class LocationBarTesting {
 public:
  // Returns the total number of page actions in the Omnibox.
  virtual int PageActionCount() = 0;

  // Returns the number of visible page actions in the Omnibox.
  virtual int PageActionVisibleCount() = 0;

  // Returns the ExtensionAction at |index|.
  virtual ExtensionAction* GetPageAction(size_t index) = 0;

  // Returns the visible ExtensionAction at |index|.
  virtual ExtensionAction* GetVisiblePageAction(size_t index) = 0;

  // Simulates a left mouse pressed on the visible page action at |index|.
  virtual void TestPageActionPressed(size_t index) = 0;

 protected:
  virtual ~LocationBarTesting() {}
};

#endif  // CHROME_BROWSER_UI_OMNIBOX_LOCATION_BAR_H_
