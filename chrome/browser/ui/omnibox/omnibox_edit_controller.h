// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_EDIT_CONTROLLER_H_
#define CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_EDIT_CONTROLLER_H_

#include "base/strings/string16.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

class CommandUpdater;
class InstantController;
class ToolbarModel;

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
  void OnAutocompleteAccept(const GURL& destination_url,
                            WindowOpenDisposition disposition,
                            ui::PageTransition transition);

  // Updates the controller, and, if |contents| is non-NULL, restores saved
  // state that the tab holds.
  virtual void Update(const content::WebContents* contents) = 0;

  // Called when anything has changed that might affect the layout or contents
  // of the views around the edit, including the text of the edit and the
  // status of any keyword- or hint-related state.
  virtual void OnChanged() = 0;

  // Called whenever the autocomplete edit gets focused.
  virtual void OnSetFocus() = 0;

  // Hides the origin chip and shows the URL.
  virtual void ShowURL() = 0;

  // Hides the origin chip while leaving the Omnibox empty.
  void HideOriginChip();

  // Shows the origin chip.  Hides the URL if it was previously shown by a call
  // to ShowURL().
  void ShowOriginChip();

  // Ends any in-progress animations related to showing/hiding the origin chip.
  // If |cancel_fade| is false, we still allow the chip itself to fade in if
  // we're ending a currently-running hide animation.
  virtual void EndOriginChipAnimations(bool cancel_fade) = 0;

  // Returns the InstantController, or NULL if instant is not enabled.
  virtual InstantController* GetInstant() = 0;

  // Returns the WebContents of the currently active tab.
  virtual content::WebContents* GetWebContents() = 0;

  virtual ToolbarModel* GetToolbarModel() = 0;
  virtual const ToolbarModel* GetToolbarModel() const = 0;

 protected:
  explicit OmniboxEditController(CommandUpdater* command_updater);
  virtual ~OmniboxEditController();

  // Hides the URL and shows the origin chip.
  virtual void HideURL() = 0;

  CommandUpdater* command_updater() { return command_updater_; }
  GURL destination_url() const { return destination_url_; }
  WindowOpenDisposition disposition() const { return disposition_; }
  ui::PageTransition transition() const { return transition_; }

 private:
  CommandUpdater* command_updater_;

  // The details necessary to open the user's desired omnibox match.
  GURL destination_url_;
  WindowOpenDisposition disposition_;
  ui::PageTransition transition_;

  DISALLOW_COPY_AND_ASSIGN(OmniboxEditController);
};

#endif  // CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_EDIT_CONTROLLER_H_
