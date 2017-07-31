// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CONTENT_RENDERER_PASSWORD_FORM_CONVERSION_UTILS_H_
#define COMPONENTS_AUTOFILL_CONTENT_RENDERER_PASSWORD_FORM_CONVERSION_UTILS_H_

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/strings/string_piece.h"
#include "components/autofill/core/common/password_form.h"
#include "components/autofill/core/common/password_form_field_prediction_map.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "url/gurl.h"

namespace blink {
class WebFormElement;
class WebFormControlElement;
class WebInputElement;
class WebLocalFrame;
}

namespace autofill {

struct PasswordForm;

// Tests whether the given form is a GAIA reauthentication form.
bool IsGaiaReauthenticationForm(const blink::WebFormElement& form);

typedef std::map<
    const blink::WebFormControlElement,
    std::pair<std::unique_ptr<base::string16>, FieldPropertiesMask>>
    FieldValueAndPropertiesMaskMap;

// Create a PasswordForm from DOM form. Webkit doesn't allow storing
// custom metadata to DOM nodes, so we have to do this every time an event
// happens with a given form and compare against previously Create'd forms
// to identify..which sucks.
// If an element of |form| has an entry in |nonscript_modified_values|, the
// associated string is used instead of the element's value to create
// the PasswordForm.
// |form_predictions| is Autofill server response, if present it's used for
// overwriting default username element selection.
std::unique_ptr<PasswordForm> CreatePasswordFormFromWebForm(
    const blink::WebFormElement& form,
    const FieldValueAndPropertiesMaskMap* nonscript_modified_values,
    const FormsPredictionsMap* form_predictions);

// Same as CreatePasswordFormFromWebForm() but for input elements that are not
// enclosed in <form> element.
std::unique_ptr<PasswordForm> CreatePasswordFormFromUnownedInputElements(
    const blink::WebLocalFrame& frame,
    const FieldValueAndPropertiesMaskMap* nonscript_modified_values,
    const FormsPredictionsMap* form_predictions);

// Checks in a case-insensitive way if the autocomplete attribute for the given
// |element| is present and has the specified |value_in_lowercase|.
bool HasAutocompleteAttributeValue(const blink::WebInputElement& element,
                                   base::StringPiece value_in_lowercase);

// Checks in a case-insensitive way if credit card autocomplete attributes for
// the given |element| are present.
bool HasCreditCardAutocompleteAttributes(const blink::WebInputElement& element);

// Returns whether the form |field| has a "password" type, but looks like a
// credit card verification field.
bool IsCreditCardVerificationPasswordField(const blink::WebInputElement& field);

// The "Realm" for the sign-on. This is scheme, host, port.
std::string GetSignOnRealm(const GURL& origin);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CONTENT_RENDERER_PASSWORD_FORM_CONVERSION_UTILS_H__
