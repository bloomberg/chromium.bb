// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_SESSION_MEDIA_METADATA_SANITIZER_H_
#define CONTENT_BROWSER_MEDIA_SESSION_MEDIA_METADATA_SANITIZER_H_

namespace media_session {
struct MediaMetadata;
}  // namespace media_session

namespace content {

class MediaMetadataSanitizer {
 public:
  // Check the sanity of |metadata|.
  static bool CheckSanity(const media_session::MediaMetadata& metadata);

  // Sanitizes |metadata| and return the result.
  static media_session::MediaMetadata Sanitize(
      const media_session::MediaMetadata& metadata);
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_SESSION_MEDIA_METADATA_SANITIZER_H_
