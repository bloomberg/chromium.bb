// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sessions/session_common_utils.h"

#include "chrome/common/url_constants.h"
#include "url/gurl.h"

bool ShouldTrackURLForRestore(const GURL& url) {
  return url.is_valid() &&
         !(url.SchemeIs(content::kChromeUIScheme) &&
           (url.host() == chrome::kChromeUIQuitHost ||
            url.host() == chrome::kChromeUIRestartHost));
}
