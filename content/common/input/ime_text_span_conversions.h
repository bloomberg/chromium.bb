// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_INPUT_IME_TEXT_SPAN_CONVERSIONS_H_
#define CONTENT_COMMON_INPUT_IME_TEXT_SPAN_CONVERSIONS_H_

#include "third_party/WebKit/public/web/WebImeTextSpan.h"
#include "ui/base/ime/ime_text_span.h"

namespace content {

blink::WebImeTextSpan::Type ConvertUiImeTextSpanTypeToWebType(
    ui::ImeTextSpan::Type type);
ui::ImeTextSpan::Type ConvertWebImeTextSpanTypeToUiType(
    blink::WebImeTextSpan::Type type);

}  // namespace content

#endif  // CONTENT_COMMON_INPUT_IME_TEXT_SPAN_CONVERSIONS_H_
