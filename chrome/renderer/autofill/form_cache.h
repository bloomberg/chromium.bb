// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_AUTOFILL_FORM_CACHE_H_
#define CHROME_RENDERER_AUTOFILL_FORM_CACHE_H_

#include <map>
#include <set>
#include <vector>

#include "base/string16.h"

struct FormData;
struct FormDataPredictions;

namespace WebKit {
class WebDocument;
class WebFrame;
class WebInputElement;
class WebSelectElement;
}

namespace autofill {

// Manages the forms in a RenderView.
class FormCache {
 public:
  FormCache();
  ~FormCache();

  // Scans the DOM in |frame| extracting and storing forms.
  // Returns a vector of the extracted forms.
  void ExtractForms(const WebKit::WebFrame& frame,
                    std::vector<FormData>* forms);

  // Resets the forms for the specified |frame|.
  void ResetFrame(const WebKit::WebFrame& frame);

  // Clears the values of all input elements in the form that contains
  // |element|.  Returns false if the form is not found.
  bool ClearFormWithElement(const WebKit::WebInputElement& element);

  // For each field in the |form|, sets the field's placeholder text to the
  // field's overall predicted type.  Also sets the title to include the field's
  // heuristic type, server type, and signature; as well as the form's signature
  // and the experiment id for the server predictions.
  bool ShowPredictions(const FormDataPredictions& form);

 private:
  // The cached web frames.
  std::set<WebKit::WebDocument> web_documents_;

  // The cached initial values for <select> elements.
  std::map<const WebKit::WebSelectElement, string16> initial_select_values_;

  // The cached initial values for checkable <input> elements.
  std::map<const WebKit::WebInputElement, bool> initial_checked_state_;

  DISALLOW_COPY_AND_ASSIGN(FormCache);
};

}  // namespace autofill

#endif  // CHROME_RENDERER_AUTOFILL_FORM_CACHE_H_
