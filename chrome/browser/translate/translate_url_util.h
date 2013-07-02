// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TRANSLATE_TRANSLATE_URL_UTIL_H_
#define CHROME_BROWSER_TRANSLATE_TRANSLATE_URL_UTIL_H_

#include "url/gurl.h"

namespace TranslateURLUtil {

// Appends Translate API Key as a part of query to a passed |url|, and returns
// GURL instance.
GURL AddApiKeyToUrl(const GURL& url);

// Appends host locale parameter as a part of query to a passed |url|, and
// returns GURL instance.
GURL AddHostLocaleToUrl(const GURL& url);

}  // namespace TranslateURLUtil

#endif  // CHROME_BROWSER_TRANSLATE_TRANSLATE_URL_UTIL_H_
