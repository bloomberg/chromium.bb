// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/omnibox/omnibox_controller.h"

#include "chrome/browser/autocomplete/autocomplete_classifier.h"
#include "chrome/browser/autocomplete/autocomplete_controller.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/ui/omnibox/omnibox_edit_model.h"


OmniboxController::OmniboxController(OmniboxEditModel* omnibox_edit_model,
                                     Profile* profile)
    : omnibox_edit_model_(omnibox_edit_model) {
  autocomplete_controller_.reset(new AutocompleteController(profile, this,
      chrome::IsInstantExtendedAPIEnabled() ?
          AutocompleteClassifier::kInstantExtendedOmniboxProviders :
          AutocompleteClassifier::kDefaultOmniboxProviders));
}

OmniboxController::~OmniboxController() {
}

void OmniboxController::OnResultChanged(bool default_match_changed) {
  omnibox_edit_model_->OnResultChanged(default_match_changed);
}
