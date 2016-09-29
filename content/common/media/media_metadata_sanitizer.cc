// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/media/media_metadata_sanitizer.h"

#include <algorithm>
#include <string>

#include "content/public/common/media_metadata.h"

namespace content {

namespace {

// Maximum length for all the strings inside the MediaMetadata when it is sent
// over IPC. The renderer process should truncate the strings before sending
// the MediaMetadata and the browser process must do the same when receiving
// it.
const size_t kMaxIPCStringLength = 4 * 1024;

// Maximum type length of Artwork, which conforms to RFC 4288
// (https://tools.ietf.org/html/rfc4288).
const size_t kMaxArtworkTypeLength = 2 * 127 + 1;

// Maximum number of artwork images inside the MediaMetadata.
const size_t kMaxNumberOfArtworkImages = 10;

// Maximum of sizes in an artwork image.
const size_t kMaxNumberOfArtworkSizes = 10;

bool CheckArtworkSrcSanity(const GURL& src) {
  if (!src.is_valid())
    return false;
  if (!src.SchemeIsHTTPOrHTTPS() && !src.SchemeIs(url::kDataScheme))
    return false;
  if (src.spec().size() > url::kMaxURLChars)
    return false;

  return true;
}

bool CheckArtworkSanity(const MediaMetadata::Artwork& artwork) {
  if (!CheckArtworkSrcSanity(artwork.src))
    return false;
  if (artwork.type.size() > kMaxArtworkTypeLength)
    return false;
  if (artwork.sizes.size() > kMaxNumberOfArtworkSizes)
    return false;

  return true;
}

// Sanitize artwork. The method should not be called if |artwork.src| is bad.
MediaMetadata::Artwork SanitizeArtwork(const MediaMetadata::Artwork& artwork) {
  MediaMetadata::Artwork sanitized_artwork;

  sanitized_artwork.src = artwork.src;
  sanitized_artwork.type = artwork.type.substr(0, kMaxArtworkTypeLength);
  for (const auto& size : artwork.sizes) {
    sanitized_artwork.sizes.push_back(size);
    if (sanitized_artwork.sizes.size() == kMaxNumberOfArtworkSizes)
      break;
  }

  return sanitized_artwork;
}

}  // anonymous namespace

bool MediaMetadataSanitizer::CheckSanity(const MediaMetadata& metadata) {
  if (metadata.title.size() > kMaxIPCStringLength)
    return false;
  if (metadata.artist.size() > kMaxIPCStringLength)
    return false;
  if (metadata.album.size() > kMaxIPCStringLength)
    return false;
  if (metadata.artwork.size() > kMaxNumberOfArtworkImages)
    return false;

  for (const auto& artwork : metadata.artwork) {
    if (!CheckArtworkSanity(artwork))
        return false;
  }

  return true;
}

MediaMetadata MediaMetadataSanitizer::Sanitize(const MediaMetadata& metadata) {
  MediaMetadata sanitized_metadata;

  sanitized_metadata.title = metadata.title.substr(0, kMaxIPCStringLength);
  sanitized_metadata.artist = metadata.artist.substr(0, kMaxIPCStringLength);
  sanitized_metadata.album = metadata.album.substr(0, kMaxIPCStringLength);

  for (const auto& artwork : metadata.artwork) {
    if (!CheckArtworkSrcSanity(artwork.src))
      continue;

    sanitized_metadata.artwork.push_back(
        CheckArtworkSanity(artwork) ? artwork : SanitizeArtwork(artwork));

    if (sanitized_metadata.artwork.size() == kMaxNumberOfArtworkImages)
      break;
  }

  return sanitized_metadata;
}

}  // namespace content
