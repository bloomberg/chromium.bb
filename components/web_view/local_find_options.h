// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEB_VIEW_LOCAL_FIND_OPTIONS_H_
#define COMPONENTS_WEB_VIEW_LOCAL_FIND_OPTIONS_H_

#include "components/web_view/public/interfaces/frame.mojom.h"
#include "mojo/public/cpp/bindings/type_converter.h"

namespace web_view {

// A local mirror of the relevant parts of mojom::FindOptions.
struct LocalFindOptions {
  bool forward;
  bool continue_last_find;
};

}  // namespace web_view

namespace mojo {

template <>
struct TypeConverter<web_view::mojom::FindOptionsPtr,
                     web_view::LocalFindOptions> {
  static web_view::mojom::FindOptionsPtr Convert(
      const web_view::LocalFindOptions& input);
};

}  // namespace mojo

#endif  // COMPONENTS_WEB_VIEW_LOCAL_FIND_OPTIONS_H_
