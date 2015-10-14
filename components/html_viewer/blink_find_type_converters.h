// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HTML_VIEWER_BLINK_FIND_TYPE_CONVERTERS_H_
#define COMPONENTS_HTML_VIEWER_BLINK_FIND_TYPE_CONVERTERS_H_

#include "components/web_view/public/interfaces/frame.mojom.h"
#include "third_party/WebKit/public/web/WebFindOptions.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/type_converter.h"

namespace mojo {

template <>
struct TypeConverter<blink::WebFindOptions, web_view::mojom::FindOptionsPtr> {
  static blink::WebFindOptions Convert(
      const web_view::mojom::FindOptionsPtr& input);
};

}  // namespace mojo

#endif  // COMPONENTS_HTML_VIEWER_BLINK_FIND_TYPE_CONVERTERS_H_
