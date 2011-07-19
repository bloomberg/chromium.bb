// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_IMPORTER_IMPORTER_UTIL_H_
#define CHROME_BROWSER_IMPORTER_IMPORTER_UTIL_H_
#pragma once

#include <cstddef>
#include <vector>

namespace importer {

// Given raw image data, decodes the icon, re-sampling to the correct size as
// necessary, and re-encodes as PNG data in the given output vector. Returns
// true on success.
bool ReencodeFavicon(const unsigned char* src_data,
                     size_t src_len,
                     std::vector<unsigned char>* png_data);

}  // namespace importer

#endif  // CHROME_BROWSER_IMPORTER_IMPORTER_UTIL_H_
