// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_AUTOFILL_EXTERNAL_DELEGATE_H_
#define CHROME_BROWSER_AUTOFILL_AUTOFILL_EXTERNAL_DELEGATE_H_
#pragma once

#include <vector>

#include "base/string16.h"

class AutofillManager;
class TabContentsWrapper;

namespace gfx {
class Rect;
}

namespace webkit_glue {
struct FormData;
struct FormField;
}  // namespace webkit_glue

// Delegate for external processing of Autocomplete and Autofill
// display and selection.
class AutofillExternalDelegate {
 public:
  virtual ~AutofillExternalDelegate();

  // When using an external Autofill delegate.  Allows Chrome to tell
  // WebKit which Autofill selection has been chosen.
  // TODO(jrg): add feedback mechanism for hover on relevant platforms.
  void SelectAutofillSuggestionAtIndex(int listIndex);

  // Records and associates a query_id with web form data.  Called
  // when the renderer posts an Autofill query to the browser. |bounds|
  // is window relative.
  virtual void OnQuery(int query_id,
                       const webkit_glue::FormData& form,
                       const webkit_glue::FormField& field,
                       const gfx::Rect& bounds) = 0;

  // Records query results.  Displays them to the user with an external
  // Autofill popup that lives completely in the browser.  Called when
  // an Autofill query result is available.
  // TODO(csharp): This should contain the logic found in
  // AutofillAgent::OnSuggestionsReturned.
  // See http://crbug.com/51644
  virtual void OnSuggestionsReturned(
      int query_id,
      const std::vector<string16>& autofill_values,
      const std::vector<string16>& autofill_labels,
      const std::vector<string16>& autofill_icons,
      const std::vector<int>& autofill_unique_ids) = 0;

  // Hide the Autofill poup.
  virtual void HideAutofillPopup() = 0;

  // Platforms that wish to implement an external Autofill delegate
  // MUST implement this.  The 1st arg is the tab contents that owns
  // this delegate; the second is the Autofill manager owned by the
  // tab contents.
  static AutofillExternalDelegate* Create(TabContentsWrapper*,
                                          AutofillManager*);

 protected:
  explicit AutofillExternalDelegate(TabContentsWrapper* tab_contents_wrapper);

 private:
  TabContentsWrapper* tab_contents_wrapper_;  // weak; owns me.

  DISALLOW_COPY_AND_ASSIGN(AutofillExternalDelegate);
};

#endif  // CHROME_BROWSER_AUTOFILL_AUTOFILL_EXTERNAL_DELEGATE_H_
