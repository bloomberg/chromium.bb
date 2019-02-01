// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_SESSION_MEDIA_METADATA_SANITIZER_H_
#define CONTENT_BROWSER_MEDIA_SESSION_MEDIA_METADATA_SANITIZER_H_

#include "third_party/blink/public/platform/modules/mediasession/media_session.mojom.h"

namespace media_session {
struct MediaMetadata;
}  // namespace media_session

namespace content {

class MediaMetadataSanitizer {
 public:
  // Converts |metadata| to a media_session::MediaMetadata object and returns
  // whether it is valid.
  static bool SanitizeAndConvert(
      const blink::mojom::SpecMediaMetadataPtr& metadata,
      media_session::MediaMetadata* metadata_out);
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_SESSION_MEDIA_METADATA_SANITIZER_H_
