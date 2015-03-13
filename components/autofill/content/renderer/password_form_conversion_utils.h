// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CONTENT_RENDERER_PASSWORD_FORM_CONVERSION_UTILS_H_
#define COMPONENTS_AUTOFILL_CONTENT_RENDERER_PASSWORD_FORM_CONVERSION_UTILS_H_

#include <map>

#include "base/memory/scoped_ptr.h"

namespace blink {
class WebFormElement;
class WebInputElement;
class WebString;
}

namespace autofill {

struct PasswordForm;

// Create a PasswordForm from DOM form. Webkit doesn't allow storing
// custom metadata to DOM nodes, so we have to do this every time an event
// happens with a given form and compare against previously Create'd forms
// to identify..which sucks.
// If an element of |form| has an entry in |nonscript_modified_values|, the
// associated string is used instead of the element's value to create
// the PasswordForm.
scoped_ptr<PasswordForm> CreatePasswordForm(
    const blink::WebFormElement& form,
    const std::map<const blink::WebInputElement, blink::WebString>*
        nonscript_modified_values);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CONTENT_RENDERER_PASSWORD_FORM_CONVERSION_UTILS_H__
