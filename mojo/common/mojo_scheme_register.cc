// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/common/mojo_scheme_register.h"

#include "url/gurl.h"
#include "url/url_util.h"

namespace mojo {

void RegisterMojoSchemes() {
  static bool registered = false;

  if (registered)
    return;

  registered = true;
  url::AddStandardScheme("mojo", url::SCHEME_WITHOUT_AUTHORITY);
  url::AddStandardScheme("exe", url::SCHEME_WITHOUT_AUTHORITY);
}

}  // namespace mojo
