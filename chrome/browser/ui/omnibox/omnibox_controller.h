// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_CONTROLLER_H_
#define CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_CONTROLLER_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string16.h"
#include "chrome/browser/autocomplete/autocomplete_controller.h"
#include "chrome/browser/autocomplete/autocomplete_controller_delegate.h"
#include "chrome/browser/autocomplete/autocomplete_match.h"

class AUtocompleteInput;
struct AutocompleteMatch;
class AutocompleteResult;
class InstantController;
class OmniboxEditModel;
class OmniboxPopupModel;
class Profile;

namespace gfx {
class Rect;
}

// This class controls the various services that can modify the content
// for the omnibox, including AutocompleteController and InstantController. It
// is responsible of updating the omnibox content.
// TODO(beaudoin): Keep on expanding this class so that OmniboxEditModel no
//     longer needs to hold any reference to AutocompleteController. Also make
//     this the point of contact between InstantController and OmniboxEditModel.
//     As the refactor progresses, keep the class comment up-to-date to
//     precisely explain what this class is doing.
class OmniboxController : public AutocompleteControllerDelegate {
 public:
  OmniboxController(OmniboxEditModel* omnibox_edit_model,
                    Profile* profile);
  virtual ~OmniboxController();

  // The |current_url| field of input is only set for mobile ports.
  void StartAutocomplete(const AutocompleteInput& input) const;

  // AutocompleteControllerDelegate:
  virtual void OnResultChanged(bool default_match_changed) OVERRIDE;

  AutocompleteController* autocomplete_controller() {
    return autocomplete_controller_.get();
  }

  // Set |current_match_| to an invalid value, indicating that we do not yet
  // have a valid match for the current text in the omnibox.
  void InvalidateCurrentMatch();

  void set_popup_model(OmniboxPopupModel* popup_model) {
    popup_ = popup_model;
  }

  // TODO(beaudoin): The edit and popup model should be siblings owned by the
  // LocationBarView, making this accessor unnecessary.
  OmniboxPopupModel* popup_model() const { return popup_; }

  const AutocompleteMatch& current_match() const { return current_match_; }

  // Turns off keyword mode for the current match.
  void ClearPopupKeywordMode() const;

  const AutocompleteResult& result() const {
    return autocomplete_controller_->result();
  }

  // TODO(beaudoin): Make private once OmniboxEditModel no longer refers to it.
  void DoPreconnect(const AutocompleteMatch& match);

 private:
  // Weak, it owns us.
  // TODO(beaudoin): Consider defining a delegate to ease unit testing.
  OmniboxEditModel* omnibox_edit_model_;

  Profile* profile_;

  OmniboxPopupModel* popup_;

  scoped_ptr<AutocompleteController> autocomplete_controller_;

  // TODO(beaudoin): This AutocompleteMatch is used to let the OmniboxEditModel
  // know what it should display. Not every field is required for that purpose,
  // but the ones specifically needed are unclear. We should therefore spend
  // some time to extract these fields and use a tighter structure here.
  AutocompleteMatch current_match_;

  DISALLOW_COPY_AND_ASSIGN(OmniboxController);
};

#endif  // CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_CONTROLLER_H_
