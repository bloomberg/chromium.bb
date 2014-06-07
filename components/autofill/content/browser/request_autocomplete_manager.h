// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CONTENT_BROWSER_REQUEST_AUTOCOMPLETE_MANAGER_H_
#define COMPONENTS_AUTOFILL_CONTENT_BROWSER_REQUEST_AUTOCOMPLETE_MANAGER_H_

#include "base/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "components/autofill/core/browser/autofill_client.h"

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
  // finished with the |result| saying if it was successful or not, and
  // |form_structure| containing the filled form data. |debug_message| will
  // be printed to the developer console.
  void ReturnAutocompleteResult(
      AutofillClient::RequestAutocompleteResult result,
      const base::string16& debug_message,
      const FormStructure* form_structure);

  // Shows the requestAutocomplete dialog for |source_url| with data from |form|
  // and calls |callback| once the interaction is complete.
  void ShowRequestAutocompleteDialog(
      const FormData& form,
      const GURL& source_url,
      const AutofillClient::ResultCallback& callback);

  // The autofill driver owns and outlives |this|.
  ContentAutofillDriver* const autofill_driver_;  // weak.

  base::WeakPtrFactory<RequestAutocompleteManager> weak_ptr_factory_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CONTENT_BROWSER_REQUEST_AUTOCOMPLETE_MANAGER_H_
