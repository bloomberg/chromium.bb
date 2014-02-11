// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/website_settings/website_settings_utils.h"

#include "content/public/common/url_constants.h"
#include "extensions/common/constants.h"
#include "url/gurl.h"

// Returns true if the passed |url| refers to an internal chrome page.
bool InternalChromePage(const GURL& url) {
  return url.SchemeIs(content::kChromeUIScheme) ||
         url.SchemeIs(extensions::kExtensionScheme);
}
