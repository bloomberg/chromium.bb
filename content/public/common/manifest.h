// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_MANIFEST_H_
#define CONTENT_PUBLIC_COMMON_MANIFEST_H_

#include <vector>

#include "base/strings/nullable_string16.h"
#include "content/common/content_export.h"
#include "third_party/WebKit/public/platform/WebScreenOrientationLockType.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"

namespace content {

// The Manifest structure is an internal representation of the Manifest file
// described in the "Manifest for Web Application" document:
// http://w3c.github.io/manifest/
struct CONTENT_EXPORT Manifest {
  enum DisplayMode {
    DISPLAY_MODE_UNSPECIFIED,
    DISPLAY_MODE_FULLSCREEN,
    DISPLAY_MODE_STANDALONE,
    DISPLAY_MODE_MINIMAL_UI,
    DISPLAY_MODE_BROWSER
  };

  // Structure representing an icon as per the Manifest specification, see:
  // http://w3c.github.io/manifest/#dfn-icon-object
  struct CONTENT_EXPORT Icon {
    Icon();
    ~Icon();

    // MUST be a valid url. If an icon doesn't have a valid URL, it will not be
    // successfully parsed, thus will not be represented in the Manifest.
    GURL src;

    // Null if the parsing failed or the field was not present. The type can be
    // any string and doesn't have to be a valid image MIME type at this point.
    // It is up to the consumer of the object to check if the type matches a
    // supported type.
    base::NullableString16 type;

    // Default value is 1.0 if the value is missing or invalid.
    double density;

    // Empty if the parsing failed, the field was not present or empty.
    // The special value "any" is represented by gfx::Size(0, 0).
    std::vector<gfx::Size> sizes;

    // Default density. Set to 1.0.
    static const double kDefaultDensity;
  };

  Manifest();
  ~Manifest();

  // Returns whether this Manifest had no attribute set. A newly created
  // Manifest is always empty.
  bool IsEmpty() const;

  // Null if the parsing failed or the field was not present.
  base::NullableString16 name;

  // Null if the parsing failed or the field was not present.
  base::NullableString16 short_name;

  // Empty if the parsing failed or the field was not present.
  GURL start_url;

  // Set to DISPLAY_MODE_UNSPECIFIED if the parsing failed or the field was not
  // present.
  DisplayMode display;

  // Set to blink::WebScreenOrientationLockDefault if the parsing failed or the
  // field was not present.
  blink::WebScreenOrientationLockType orientation;

  // Empty if the parsing failed, the field was not present, empty or all the
  // icons inside the JSON array were invalid.
  std::vector<Icon> icons;

  // Maximum length for all the strings inside the Manifest when it is sent over
  // IPC. The renderer process should truncate the strings before sending the
  // Manifest and the browser process must do the same when receiving it.
  static const size_t kMaxIPCStringLength;
};

} // namespace content

#endif // CONTENT_PUBLIC_COMMON_MANIFEST_H_
