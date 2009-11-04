// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_FORM_STRUCTURE_H_
#define CHROME_BROWSER_AUTOFILL_FORM_STRUCTURE_H_

#include <string>
#include <vector>

#include "base/scoped_ptr.h"
#include "chrome/browser/autofill/autofill_field.h"
#include "googleurl/src/gurl.h"

namespace webkit_glue {
class FormFieldValues;
}

enum RequestMethod {
  GET,
  POST
};

// FormStructure stores a single HTML form together with the values entered
// in the fields along with additional information needed by AutoFill.
class FormStructure {
 public:
  explicit FormStructure(const webkit_glue::FormFieldValues& values);

  // Encodes the XML upload request from this FormStructure.
  bool EncodeUploadRequest(bool auto_fill_used, std::string* encoded_xml) const;

  // The unique signature for this form, composed of the target url domain,
  // the form name, and the form field names in a 64-bit hash.
  std::string FormSignature() const;

  // Runs a quick heuristic to rule out pages but obviously not auto-fillable,
  // like google/yahoo/msn search, etc.
  bool IsAutoFillable() const;

 private:
  // The name of the form.
  std::string form_name_;

  // The source URL.
  GURL source_url_;

  // The target URL.
  GURL target_url_;

  // A vector of all the input fields in the form.
  std::vector<AutoFillField> fields_;

  // The names of the form input elements, that are part of the form signature.
  // The string starts with "&" and the names are also separated by the "&"
  // character. E.g.: "&form_input1_name&form_input2_name&...&form_inputN_name"
  std::string form_signature_field_names_;

  // GET or POST.
  RequestMethod method_;
};

#endif  // CHROME_BROWSER_AUTOFILL_FORM_STRUCTURE_H_
