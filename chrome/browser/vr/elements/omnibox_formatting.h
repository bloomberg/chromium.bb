// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_ELEMENTS_OMNIBOX_FORMATTING_H_
#define CHROME_BROWSER_VR_ELEMENTS_OMNIBOX_FORMATTING_H_

#include <memory>

#include "chrome/browser/vr/elements/text.h"
#include "chrome/browser/vr/model/color_scheme.h"
#include "components/omnibox/browser/autocomplete_match.h"

namespace vr {

TextFormatting ConvertClassification(
    const ACMatchClassifications& classifications,
    size_t text_length,
    const ColorScheme& color_scheme);

}  // namespace vr

#endif  // CHROME_BROWSER_VR_ELEMENTS_OMNIBOX_FORMATTING_H_
