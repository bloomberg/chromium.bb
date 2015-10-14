// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/html_viewer/blink_find_type_converters.h"

namespace mojo {

blink::WebFindOptions
TypeConverter<blink::WebFindOptions, web_view::mojom::FindOptionsPtr>::Convert(
    const web_view::mojom::FindOptionsPtr& input) {
  blink::WebFindOptions output;
  output.forward = input->forward;
  output.findNext = input->continue_last_find;
  return output;
}

}  // namespace mojo
