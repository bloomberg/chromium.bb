// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/web_view/local_find_options.h"

namespace mojo {

web_view::mojom::FindOptionsPtr
TypeConverter<web_view::mojom::FindOptionsPtr, web_view::LocalFindOptions>::
    Convert(const web_view::LocalFindOptions& input) {
  web_view::mojom::FindOptionsPtr output = web_view::mojom::FindOptions::New();
  output->forward = input.forward;
  output->continue_last_find = input.continue_last_find;
  return output.Pass();
}

}  // namespace mojo
