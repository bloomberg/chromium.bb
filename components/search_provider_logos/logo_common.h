// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEARCH_PROVIDER_LOGOS_LOGO_COMMON_H_
#define COMPONENTS_SEARCH_PROVIDER_LOGOS_LOGO_COMMON_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/memory/ref_counted_memory.h"
#include "base/time/time.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "url/gurl.h"

namespace search_provider_logos {

// The maximum number of milliseconds that a logo can be cached.
extern const int64_t kMaxTimeToLiveMS;

struct LogoMetadata {
  LogoMetadata();
  LogoMetadata(const LogoMetadata& other);
  ~LogoMetadata();

  // For use by the client ----------------------------------------------------

  // The URL to load when the logo is clicked.
  GURL on_click_url;
  // The accessibility text for the logo.
  std::string alt_text;
  // The mime type of the logo image.
  std::string mime_type;
  // The URL for an animated image to display when the call to action logo is
  // clicked. If |animated_url| is not empty, |encoded_image| refers to a call
  // to action image.
  GURL animated_url;

  // For use by LogoTracker ---------------------------------------------------

  // The URL from which the logo was downloaded (without the fingerprint param).
  GURL source_url;
  // A fingerprint (i.e. hash) identifying the logo. Used when revalidating the
  // logo with the server.
  std::string fingerprint;
  // Whether the logo can be shown optimistically after it's expired while a
  // fresh logo is being downloaded.
  bool can_show_after_expiration;
  // When the logo expires. After this time, the logo will not be used and will
  // be deleted.
  base::Time expiration_time;
};

struct EncodedLogo {
  EncodedLogo();
  EncodedLogo(const EncodedLogo& other);
  ~EncodedLogo();

  // The jpeg- or png-encoded image.
  scoped_refptr<base::RefCountedString> encoded_image;
  // Metadata about the logo.
  LogoMetadata metadata;
};

struct Logo {
  Logo();
  ~Logo();

  // The logo image.
  SkBitmap image;
  // Metadata about the logo.
  LogoMetadata metadata;
};

// Parses the response from the server and returns it as an EncodedLogo. Returns
// null if the response is invalid.
using ParseLogoResponse = base::Callback<std::unique_ptr<EncodedLogo>(
    std::unique_ptr<std::string> response,
    base::Time response_time,
    bool* parsing_failed)>;

// Encodes the fingerprint of the cached logo in the logo URL. This enables the
// server to verify whether the cached logo is up to date.
using AppendQueryparamsToLogoURL =
    base::Callback<GURL(const GURL& logo_url, const std::string& fingerprint)>;

}  // namespace search_provider_logos

#endif  // COMPONENTS_SEARCH_PROVIDER_LOGOS_LOGO_COMMON_H_
