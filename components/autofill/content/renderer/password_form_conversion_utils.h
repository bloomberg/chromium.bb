// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CONTENT_RENDERER_PASSWORD_FORM_CONVERSION_UTILS_H_
#define COMPONENTS_AUTOFILL_CONTENT_RENDERER_PASSWORD_FORM_CONVERSION_UTILS_H_

#include "base/memory/scoped_ptr.h"

namespace blink {
class WebFormElement;
}

namespace autofill {

struct PasswordForm;

// Create a PasswordForm from DOM form. Webkit doesn't allow storing
// custom metadata to DOM nodes, so we have to do this every time an event
// happens with a given form and compare against previously Create'd forms
// to identify..which sucks.
scoped_ptr<PasswordForm> CreatePasswordForm(
    const blink::WebFormElement& form);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CONTENT_RENDERER_PASSWORD_FORM_CONVERSION_UTILS_H__
