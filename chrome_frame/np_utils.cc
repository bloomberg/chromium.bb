// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#define NO_NSPR_10_SUPPORT
#include "chrome_frame/np_utils.h"

#include "chrome_frame/np_browser_functions.h"

namespace np_utils {

std::string GetLocation(NPP instance, NPObject* window) {
  if (!window) {
    // Can fail if the browser is closing (seen in Opera).
    return "";
  }

  std::string result;
  ScopedNpVariant href;
  ScopedNpVariant location;

  bool ok = npapi::GetProperty(instance,
                               window,
                               npapi::GetStringIdentifier("location"),
                               &location);
  DCHECK(ok);
  DCHECK(location.type == NPVariantType_Object);

  if (ok) {
    ok = npapi::GetProperty(instance,
                            location.value.objectValue,
                            npapi::GetStringIdentifier("href"),
                            &href);
    DCHECK(ok);
    DCHECK(href.type == NPVariantType_String);
    if (ok) {
      result.assign(href.value.stringValue.UTF8Characters,
                    href.value.stringValue.UTF8Length);
    }
  }

  return result;
}

}  // namespace np_utils
