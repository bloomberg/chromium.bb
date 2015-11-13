// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HTML_VIEWER_BLINK_TEXT_INPUT_TYPE_CONVERTERS_H_
#define COMPONENTS_HTML_VIEWER_BLINK_TEXT_INPUT_TYPE_CONVERTERS_H_

#include "mojo/public/cpp/bindings/type_converter.h"
#include "third_party/WebKit/public/web/WebTextInputType.h"
#include "ui/mojo/ime/text_input_state.mojom.h"

namespace mojo {

template <>
struct TypeConverter<TextInputType, blink::WebTextInputType> {
  static TextInputType Convert(const blink::WebTextInputType& input);
};

}  // namespace mojo

#endif  // COMPONENTS_HTML_VIEWER_BLINK_TEXT_INPUT_TYPE_CONVERTERS_H_
