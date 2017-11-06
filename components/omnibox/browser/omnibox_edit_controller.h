// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_EDIT_CONTROLLER_H_
#define COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_EDIT_CONTROLLER_H_

#include "base/macros.h"
#include "base/strings/string16.h"
#include "components/omnibox/browser/autocomplete_match_type.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

class ToolbarModel;

class OmniboxEditController {
 public:
  virtual void OnAutocompleteAccept(const GURL& destination_url,
                                    WindowOpenDisposition disposition,
                                    ui::PageTransition transition,
                                    AutocompleteMatchType::Type match_type);

  virtual void OnInputInProgress(bool in_progress) = 0;

  // Called when anything has changed that might affect the layout or contents
  // of the views around the edit, including the text of the edit and the
  // status of any keyword- or hint-related state.
  virtual void OnChanged() = 0;

  virtual ToolbarModel* GetToolbarModel() = 0;
  virtual const ToolbarModel* GetToolbarModel() const = 0;

  // Called to search for a tab with given URL within the profile.  If found,
  // the tab is switched to if it is not the current tab, the current tab is
  // closed if requested, and true is returned; returns false if not found.
  virtual bool SwitchToTabWithURL(const std::string& url, bool close_this);

 protected:
  OmniboxEditController();
  virtual ~OmniboxEditController();

  GURL destination_url() const { return destination_url_; }
  WindowOpenDisposition disposition() const { return disposition_; }
  ui::PageTransition transition() const { return transition_; }

 private:
  // The details necessary to open the user's desired omnibox match.
  GURL destination_url_;
  WindowOpenDisposition disposition_;
  ui::PageTransition transition_;

  DISALLOW_COPY_AND_ASSIGN(OmniboxEditController);
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_EDIT_CONTROLLER_H_
