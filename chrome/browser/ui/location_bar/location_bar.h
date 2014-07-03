// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_LOCATION_BAR_LOCATION_BAR_H_
#define CHROME_BROWSER_UI_LOCATION_BAR_LOCATION_BAR_H_

#include "content/public/common/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

class ExtensionAction;
class LocationBarTesting;
class OmniboxView;
class Profile;

namespace content {
class WebContents;
}

// The LocationBar class is a virtual interface, defining access to the
// window's location bar component.  This class exists so that cross-platform
// components like the browser command system can talk to the platform
// specific implementations of the location bar control.  It also allows the
// location bar to be mocked for testing.
class LocationBar {
 public:
  explicit LocationBar(Profile* profile);

  // Shows the first run bubble anchored to the location bar.
  virtual void ShowFirstRunBubble() = 0;

  // The details necessary to open the user's desired omnibox match.
  virtual GURL GetDestinationURL() const = 0;
  virtual WindowOpenDisposition GetWindowOpenDisposition() const = 0;
  virtual content::PageTransition GetPageTransition() const = 0;

  // Accepts the current string of text entered in the location bar.
  virtual void AcceptInput() = 0;

  // Focuses the location bar.  Optionally also selects its contents.
  virtual void FocusLocation(bool select_all) = 0;

  // Clears the location bar, inserts an annoying little "?" turd and sets
  // focus to it.
  virtual void FocusSearch() = 0;

  // Updates the state of the images showing the content settings status.
  virtual void UpdateContentSettingsIcons() = 0;

  // Updates the password icon and pops up a bubble from the icon if needed.
  virtual void UpdateManagePasswordsIconAndBubble() = 0;

  // Updates the state of the page actions.
  virtual void UpdatePageActions() = 0;

  // Called when the page-action data needs to be refreshed, e.g. when an
  // extension is unloaded or crashes.
  virtual void InvalidatePageActions() = 0;

  // Updates the state of the button to open a PDF in Adobe Reader.
  virtual void UpdateOpenPDFInReaderPrompt() = 0;

  // Updates the generated credit card view. This view serves as an anchor for
  // the generated credit card bubble, which can show on successful generation
  // of a new credit card number.
  virtual void UpdateGeneratedCreditCardView() = 0;

  // Saves the state of the location bar to the specified WebContents, so that
  // it can be restored later. (Done when switching tabs).
  virtual void SaveStateToContents(content::WebContents* contents) = 0;

  // Reverts the location bar.  The bar's permanent text will be shown.
  virtual void Revert() = 0;

  virtual const OmniboxView* GetOmniboxView() const = 0;
  virtual OmniboxView* GetOmniboxView() = 0;

  // Returns a pointer to the testing interface.
  virtual LocationBarTesting* GetLocationBarForTesting() = 0;

  Profile* profile() { return profile_; }

 protected:
  virtual ~LocationBar();

  // Checks if any extension has requested that the bookmark star be hidden.
  bool IsBookmarkStarHiddenByExtension() const;

 private:
  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(LocationBar);
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

  // Returns whether or not the bookmark star decoration is visible.
  virtual bool GetBookmarkStarVisibility() = 0;

 protected:
  virtual ~LocationBarTesting() {}
};

#endif  // CHROME_BROWSER_UI_LOCATION_BAR_LOCATION_BAR_H_
