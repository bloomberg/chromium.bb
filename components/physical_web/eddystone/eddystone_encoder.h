// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PHYSICAL_WEB_EDDYSTONE_EDDYSTONE_ENCODER_H_
#define COMPONENTS_PHYSICAL_WEB_EDDYSTONE_EDDYSTONE_ENCODER_H_

#include <string>
#include <vector>

namespace physical_web {

const size_t kEmptyUrl = static_cast<size_t>(-1);
const size_t kInvalidUrl = static_cast<size_t>(-2);
const size_t kInvalidFormat = static_cast<size_t>(-3);
const size_t kNullEncodedUrl = static_cast<size_t>(-4);

/*
 * EncodeUrl takes a URL in the form of a std::string and
 * a pointer to a uint8_t vector to populate with the eddystone
 * encoding of the URL.
 * Returns:
 *   kEmptyUrl If the URL parameter is empty.
 *   kInvalidUrl If the URL parameter is not a valid URL.
 *   kInvalidFormat If the URL parameter is not a standard HTTPS/HTTP URL.
 *   kNullEncodedUrl If the encoded_url vector is NULL.
 *   Length of encoded URL.
 * Eddystone spec can be found here:
 * https://github.com/google/eddystone/blob/master/protocol-specification.md
 */
size_t EncodeUrl(const std::string& url, std::vector<uint8_t>* encoded_url);
}

#endif  // COMPONENTS_PHYSICAL_WEB_EDDYSTONE_EDDYSTONE_ENCODER_H_
