// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/extension_urls.h"

#include "extensions/common/constants.h"
#include "url/gurl.h"

namespace extensions {

bool IsSourceFromAnExtension(const base::string16& source) {
  return GURL(source).SchemeIs(kExtensionScheme);
}

}  // namespace extensions
