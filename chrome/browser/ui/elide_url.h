// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file defines utility functions for eliding URLs.

#ifndef CHROME_BROWSER_UI_ELIDE_URL_H_
#define CHROME_BROWSER_UI_ELIDE_URL_H_

#include <string>

#include "base/strings/string16.h"

class GURL;

namespace gfx {
class FontList;
}

// This function takes a GURL object and elides it. It returns a string
// which composed of parts from subdomain, domain, path, filename and query.
// A "..." is added automatically at the end if the elided string is bigger
// than the |available_pixel_width|. For |available_pixel_width| == 0, a
// formatted, but un-elided, string is returned. |languages| is a comma
// separated list of ISO 639 language codes and is used to determine what
// characters are understood by a user. It should come from
// |prefs::kAcceptLanguages|.
//
// Note: in RTL locales, if the URL returned by this function is going to be
// displayed in the UI, then it is likely that the string needs to be marked
// as an LTR string (using base::i18n::WrapStringWithLTRFormatting()) so that it
// is displayed properly in an RTL context. Please refer to
// http://crbug.com/6487 for more information.
base::string16 ElideUrl(const GURL& url,
                        const gfx::FontList& font_list,
                        float available_pixel_width,
                        const std::string& languages);

// This function takes a GURL object and elides the host to fit within
// the given width. The function will never elide past the TLD+1 point,
// but after that, will leading-elide the domain name to fit the width.
// Example: http://sub.domain.com ---> "...domain.com", or "...b.domain.com"
// depending on the width.
base::string16 ElideHost(const GURL& host_url,
                         const gfx::FontList& font_list,
                         float available_pixel_width);

#endif  // CHROME_BROWSER_UI_ELIDE_URL_H_
