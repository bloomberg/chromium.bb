// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/color_suggestion.h"

namespace content {

ColorSuggestion::ColorSuggestion(const SkColor& color,
                                 const base::string16& label)
    : color(color), label(label) {}

}  // namespace content
