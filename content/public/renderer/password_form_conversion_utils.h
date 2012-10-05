// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_RENDERER_PASSWORD_FORM_CONVERSION_UTILS_H_
#define CONTENT_PUBLIC_RENDERER_PASSWORD_FORM_CONVERSION_UTILS_H_

#include "base/memory/scoped_ptr.h"
#include "content/common/content_export.h"

namespace WebKit {
class WebFormElement;
}

namespace content {

struct PasswordForm;

// Create a PasswordForm from DOM form. Webkit doesn't allow storing
// custom metadata to DOM nodes, so we have to do this every time an event
// happens with a given form and compare against previously Create'd forms
// to identify..which sucks.
CONTENT_EXPORT scoped_ptr<PasswordForm> CreatePasswordForm(
    const WebKit::WebFormElement& form);

}  // namespace content

#endif  // CONTENT_PUBLIC_RENDERER_PASSWORD_FORM_CONVERSION_UTILS_H__
