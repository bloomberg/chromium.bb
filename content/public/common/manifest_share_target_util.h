// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_MANIFEST_SHARE_TARGET_UTIL_H_
#define CONTENT_PUBLIC_COMMON_MANIFEST_SHARE_TARGET_UTIL_H_

#include <string>

#include "base/strings/string_piece.h"
#include "content/common/content_export.h"

class GURL;

namespace content {

// Determines whether |url_template| is valid; that is, whether
// ReplaceWebShareUrlPlaceholders() would succeed for the given template.
CONTENT_EXPORT bool ValidateWebShareUrlTemplate(const GURL& url_template);

// Writes to |url_template_filled|, a copy of |url_template| with all
// instances of "{title}", "{text}", and "{url}" in the query and fragment
// parts of the URL replaced with |title|, |text|, and |url| respectively.
// Replaces instances of "{X}" where "X" is any string besides "title",
// "text", and "url", with an empty string, for forwards compatibility.
// Returns false, if there are badly nested placeholders.
// This includes any case in which two "{" occur before a "}", or a "}"
// occurs with no preceding "{".
CONTENT_EXPORT bool ReplaceWebShareUrlPlaceholders(const GURL& url_template,
                                                   base::StringPiece title,
                                                   base::StringPiece text,
                                                   const GURL& share_url,
                                                   GURL* url_template_filled);

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_MANIFEST_SHARE_TARGET_UTIL_H_
