// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CONTENT_BROWSER_REQUEST_AUTOCOMPLETE_MANAGER_H_
#define COMPONENTS_AUTOFILL_CONTENT_BROWSER_REQUEST_AUTOCOMPLETE_MANAGER_H_

#include "base/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "third_party/WebKit/public/web/WebFormElement.h"

class GURL;

namespace autofill {

class ContentAutofillDriver;
struct FormData;
class FormStructure;

// Driver for the requestAutocomplete flow.
class RequestAutocompleteManager {
 public:
  explicit RequestAutocompleteManager(ContentAutofillDriver* autofill_driver);
  ~RequestAutocompleteManager();

  // Requests an interactive autocomplete UI to be shown for |frame_url| with
  // |form|.
  void OnRequestAutocomplete(const FormData& form, const GURL& frame_url);

  // Requests that any running interactive autocomplete be cancelled.
  void OnCancelRequestAutocomplete();

 private:
  // Tells the renderer that the current interactive autocomplete dialog
  // finished with the |result| saying if it was successfull or not, and
  // |form_data| containing the filled form data.
  void ReturnAutocompleteResult(
      blink::WebFormElement::AutocompleteResult result,
      const FormData& form_data);

  // Shows the requestAutocomplete dialog for |source_url| with data from |form|
  // and calls |callback| once the interaction is complete.
  void ShowRequestAutocompleteDialog(
      const FormData& form,
      const GURL& source_url,
      const base::Callback<void(const FormStructure*)>& callback);

  // If |result| is not null, derives a FormData object from it and passes it
  // back to the page along an |AutocompleteResultSuccess| result. Otherwise, it
  // passes to the page an empty FormData along an
  // |AutocompleteResultErrorCancel| result.
  void ReturnAutocompleteData(const FormStructure* result);

  // The autofill driver owns and outlives |this|.
  ContentAutofillDriver* const autofill_driver_;  // weak.

  base::WeakPtrFactory<RequestAutocompleteManager> weak_ptr_factory_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CONTENT_BROWSER_REQUEST_AUTOCOMPLETE_MANAGER_H_
