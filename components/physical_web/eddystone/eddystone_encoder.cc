// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/physical_web/eddystone/eddystone_encoder.h"

#include <string>
#include <vector>

#include "url/gurl.h"

namespace {
const char* kPrefixes[] = {"http://www.", "https://www.", "http://",
                           "https://"};
const size_t kNumPrefixes = sizeof(kPrefixes) / sizeof(char*);

const char* kSuffixes[] = {".com/", ".org/", ".edu/", ".net/", ".info/",
                           ".biz/", ".gov/", ".com",  ".org",  ".edu",
                           ".net",  ".info", ".biz",  ".gov"};
const size_t kNumSuffixes = sizeof(kSuffixes) / sizeof(char*);
}

namespace physical_web {
size_t EncodeUrl(const std::string& url, std::vector<uint8_t>* encoded_url) {
  /*
   * Sanitize input.
   */

  if (encoded_url == nullptr)
    return kNullEncodedUrl;
  if (url.empty())
    return kEmptyUrl;

  GURL gurl(url);
  if (!gurl.is_valid())
    return kInvalidUrl;
  if (gurl.HostIsIPAddress() || !gurl.SchemeIsHTTPOrHTTPS())
    return kInvalidFormat;

  /*
   * Necessary Declarations.
   */
  const char* raw_url_position = url.c_str();

  size_t prefix_lengths[kNumPrefixes];
  size_t suffix_lengths[kNumSuffixes];
  for (size_t i = 0; i < kNumPrefixes; i++) {
    prefix_lengths[i] = strlen(kPrefixes[i]);
  }
  for (size_t i = 0; i < kNumSuffixes; i++) {
    suffix_lengths[i] = strlen(kSuffixes[i]);
  }

  /*
   * Handle prefix.
   */
  for (size_t i = 0; i < kNumPrefixes; i++) {
    size_t prefix_len = prefix_lengths[i];
    if (strncmp(raw_url_position, kPrefixes[i], prefix_len) == 0) {
      encoded_url->push_back(static_cast<uint8_t>(i));
      raw_url_position += prefix_len;
      break;
    }
  }

  /*
   * Handle suffixes.
   */
  while (*raw_url_position) {
    /* check for suffix match */
    size_t i;
    for (i = 0; i < kNumSuffixes; i++) {
      size_t suffix_len = suffix_lengths[i];
      if (strncmp(raw_url_position, kSuffixes[i], suffix_len) == 0) {
        encoded_url->push_back(static_cast<uint8_t>(i));
        raw_url_position += suffix_len;
        break; /* from the for loop for checking against suffixes */
      }
    }
    /* This is the default case where we've got an ordinary character which
     * doesn't match a suffix. */
    if (i == kNumSuffixes) {
      encoded_url->push_back(*raw_url_position);
      raw_url_position++;
    }
  }

  return encoded_url->size();
}
}
