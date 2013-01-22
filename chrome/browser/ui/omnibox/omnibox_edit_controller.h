// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_EDIT_CONTROLLER_H_
#define CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_EDIT_CONTROLLER_H_

#include "base/string16.h"
#include "content/public/common/page_transition_types.h"
#include "ui/base/window_open_disposition.h"

class GURL;
class InstantController;

namespace content {
class WebContents;
}

namespace gfx {
class Image;
}

// I am in hack-and-slash mode right now.
// http://code.google.com/p/chromium/issues/detail?id=6772

// Embedders of an AutocompleteEdit widget must implement this class.
class OmniboxEditController {
 public:
  // When the user presses enter or selects a line with the mouse, this
  // function will get called synchronously with the url to open and
  // disposition and transition to use when opening it.
  //
  // |alternate_nav_url|, if non-empty, contains the alternate navigation URL
  // for |url|, which the controller can check for existence.  See comments on
  // AutocompleteResult::GetAlternateNavURL().
  virtual void OnAutocompleteAccept(const GURL& url,
                                    WindowOpenDisposition disposition,
                                    content::PageTransition transition,
                                    const GURL& alternate_nav_url) = 0;

  // Called when anything has changed that might affect the layout or contents
  // of the views around the edit, including the text of the edit and the
  // status of any keyword- or hint-related state.
  virtual void OnChanged() = 0;

  // Called when the selection of the OmniboxView changes.
  virtual void OnSelectionBoundsChanged() = 0;

  // Called whenever the user starts or stops an input session (typing,
  // interacting with the edit, etc.).  When user input is not in progress,
  // the edit is guaranteed to be showing the permanent text.
  virtual void OnInputInProgress(bool in_progress) = 0;

  // Called whenever the autocomplete edit is losing focus.
  virtual void OnKillFocus() = 0;

  // Called whenever the autocomplete edit gets focused.
  virtual void OnSetFocus() = 0;

  // Returns the favicon of the current page.
  virtual gfx::Image GetFavicon() const = 0;

  // Returns the title of the current page.
  virtual string16 GetTitle() const = 0;

  // Returns the InstantController, or NULL if instant is not enabled.
  virtual InstantController* GetInstant() = 0;

  // Returns the WebContents of the currently active tab.
  virtual content::WebContents* GetWebContents() const = 0;

 protected:
  virtual ~OmniboxEditController() {}
};

#endif  // CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_EDIT_CONTROLLER_H_
