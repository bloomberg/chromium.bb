// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/omnibox/location_bar.h"

#include "chrome/browser/autocomplete/autocomplete_edit.h"
#include "chrome/browser/autocomplete/autocomplete_edit_view.h"
#include "chrome/browser/autocomplete/autocomplete_popup_model.h"
#include "chrome/browser/instant/instant_controller.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"

// static
void LocationBar::UpdateInstant(InstantController* instant,
                                TabContentsWrapper* tab,
                                AutocompleteEditView* edit,
                                string16* suggested_text) {
  if (edit->model()->user_input_in_progress() &&
      edit->model()->popup_model()->IsOpen()) {
    AutocompleteMatch current_match = edit->model()->CurrentMatch();
    if (current_match.destination_url == edit->model()->PermanentURL()) {
      // The destination is the same as the current url. This typically happens
      // if the user presses the down error in the omnibox, in which case we
      // don't want to load a preview.
      instant->DestroyPreviewContentsAndLeaveActive();
    } else {
      instant->Update(tab, edit->model()->CurrentMatch(), edit->GetText(),
                      edit->model()->UseVerbatimInstant(), suggested_text);
    }
  } else {
    instant->DestroyPreviewContents();
  }
  if (!instant->MightSupportInstant())
    edit->model()->FinalizeInstantQuery(string16(), string16(), false);
}
