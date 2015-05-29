// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CONTENT_RENDERER_PASSWORD_FORM_CONVERSION_UTILS_H_
#define COMPONENTS_AUTOFILL_CONTENT_RENDERER_PASSWORD_FORM_CONVERSION_UTILS_H_

#include <map>

#include "base/memory/scoped_ptr.h"
#include "components/autofill/core/common/password_form_field_prediction_map.h"
#include "url/gurl.h"

namespace blink {
class WebDocument;
class WebFormElement;
class WebInputElement;
class WebString;
}

namespace autofill {

struct FormData;
struct FormFieldData;
struct PasswordForm;

// Helper functions to assist in getting the canonical form of the action and
// origin. The action will proplerly take into account <BASE>, and both will
// strip unnecessary data (e.g. query params and HTTP credentials).
GURL GetCanonicalActionForForm(const blink::WebFormElement& form);
GURL GetCanonicalOriginForDocument(const blink::WebDocument& document);

// Create a PasswordForm from DOM form. Webkit doesn't allow storing
// custom metadata to DOM nodes, so we have to do this every time an event
// happens with a given form and compare against previously Create'd forms
// to identify..which sucks.
// If an element of |form| has an entry in |nonscript_modified_values|, the
// associated string is used instead of the element's value to create
// the PasswordForm.
// |form_predictions| is Autofill server response, if present it's used for
// overwriting default username element selection.
scoped_ptr<PasswordForm> CreatePasswordForm(
    const blink::WebFormElement& form,
    const std::map<const blink::WebInputElement, blink::WebString>*
        nonscript_modified_values,
    const std::map<autofill::FormData,
                   autofill::PasswordFormFieldPredictionMap>* form_predictions);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CONTENT_RENDERER_PASSWORD_FORM_CONVERSION_UTILS_H__
